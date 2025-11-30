"""
core/simulator.py

Núcleo da Simulação da Mina.

Responsável por:
    - Manter um TruckState interno (estado físico "ideal", sem ruído).
    - Atualizar a dinâmica de posição, ângulo e temperatura do caminhão
      a partir dos atuadores (o_aceleracao, o_direcao) por equações de
      diferenças, simulando inércia de movimentação.
    - Gerar leituras de sensores com ruído gaussiano de média nula.
    - Permitir configuração de passo de tempo e de ruído por variável.
    - Notificar observadores (UI, publisher MQTT, logger) via callbacks.

Este módulo NÃO depende de PyQt nem de MQTT.
A camada de UI deverá usar, por exemplo, um QTimer para chamar
    Simulator.step()
periodicamente e repassar as atualizações para widgets/sinais Qt.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any, Callable, Dict, List, Optional, Tuple
from model.truck_state import TruckState
from model.dynamics import update_dynamics, AMBIENT_TEMP_C
from core.random_sources import NoiseConfig, apply_noise, get_default_noise_configs

# Tipo de callback chamado a cada passo de simulação:
#   callback(state_fisico: TruckState, sensores_ruidosos: Dict[str, Any]) -> None
StateCallback = Callable[[TruckState, Dict[str, Any]], None]
# Função que fornece comandos de atuadores:
#   source() -> (o_aceleracao, o_direcao)
ActuatorSource = Callable[[], Tuple[float, float]]

@dataclass
class SimulatorConfig:
    """
    Configuração básica do simulador.

    Campos:
        dt_default   : passo de tempo padrão [s], usado se step() for chamado sem dt.
        ambient_temp : temperatura ambiente [°C] usada na dinâmica térmica.
        quantize     : se True, exporta sensores quantizados (int), conforme Tabela 1
    """
    dt_default: float = 0.05  # 50 ms
    ambient_temp: float = AMBIENT_TEMP_C


class Simulator:
    """
    Núcleo da Simulação da Mina.

    Mantém um estado físico interno (TruckState) e, a cada passo:
        1) Atualiza a dinâmica em função dos atuadores (o_aceleracao, o_direcao).
        2) Gera um dicionário de sensores com ruído de média nula.
        3) Invoca todos os callbacks registrados com (estado_fisico, sensores_ruidosos).

    Integração com o restante do sistema:
        - A UI pode atualizar `self.state.o_aceleracao` e `self.state.o_direcao`
          diretamente ou via set_actuators().
        - Módulos de falha (model.faults) podem modificar self.state antes da chamada
          a step(), para injetar falhas elétricas/hidráulicas/superaquecimento
        - O publisher MQTT pode registrar um callback para publicar o dicionário de
          sensores em JSON, seguindo os tópicos da Etapa 2.
    """

    def __init__(
        self,
        state: Optional[TruckState] = None,
        config: Optional[SimulatorConfig] = None,
        noise_configs: Optional[Dict[str, NoiseConfig]] = None,
    ) -> None:
        # Estado físico "ideal" (sem ruído de medição aplicado)
        self.state: TruckState = state or TruckState()

        # Configuração geral
        self.config: SimulatorConfig = config or SimulatorConfig()

        # Tempo de simulação acumulado [s]
        self.sim_time: float = 0.0

        # Flag de execução: se False, step() não atualiza estado
        self._running: bool = False

        # Configurações de ruído por variável de sensor
        # chaves típicas: 'i_posicao_x', 'i_posicao_y', 'i_angulo_x', 'i_temperatura'
        self._noise_configs: Dict[str, NoiseConfig] = (
            noise_configs.copy() if noise_configs is not None
            else get_default_noise_configs()
        )

        # Lista de callbacks de observadores
        self._callbacks: List[StateCallback] = []
        # Fonte externa de atuadores (ex.: MQTT, interface local)
        self._actuator_source: Optional[ActuatorSource] = None
    # ----------------------------------------------------------------------
    # Controle de execução
    # ----------------------------------------------------------------------

    def start(self) -> None:
        """Coloca o simulador em modo 'rodando'."""
        self._running = True

    def stop(self) -> None:
        """Coloca o simulador em modo 'parado'."""
        self._running = False

    @property
    def is_running(self) -> bool:
        """Indica se o simulador está em execução."""
        return self._running

    # ----------------------------------------------------------------------
    # Configuração de tempo e ruído
    # ----------------------------------------------------------------------

    def set_update_period(self, dt: float) -> None:
        """
        Ajusta o passo de tempo padrão (dt_default) da simulação, em segundos.

        A UI pode mapear o campo "Período (ms)" para este valor:
            dt = periodo_ms / 1000.0
        """
        if dt <= 0.0:
            raise ValueError("dt deve ser positivo.")
        self.config.dt_default = dt

    def set_noise_config(self, var_name: str, noise_config: NoiseConfig) -> None:
        """
        Atualiza a configuração de ruído de uma variável específica
        (ex.: 'i_posicao_x', 'i_temperatura').
        """
        self._noise_configs[var_name] = noise_config

    def get_noise_config(self, var_name: str) -> Optional[NoiseConfig]:
        """
        Retorna a configuração de ruído para uma variável, se existir.
        """
        return self._noise_configs.get(var_name)

    def set_all_noise_enabled(self, enabled: bool) -> None:
        """
        Habilita/desabilita ruído em todos os sensores conhecidos.

        Útil para debug ou comparação entre casos "com" e "sem" ruído.
        """
        for cfg in self._noise_configs.values():
            cfg.enabled = enabled

    # ----------------------------------------------------------------------
    # Registro de callbacks
    # ----------------------------------------------------------------------

    def register_callback(self, callback: StateCallback) -> None:
        """
        Registra um callback a ser chamado a cada passo de simulação.

        Assinatura esperada:
            callback(state: TruckState, sensors: Dict[str, Any]) -> None
        """
        self._callbacks.append(callback)

    # ----------------------------------------------------------------------
    # Fonte externa de atuadores
    # ----------------------------------------------------------------------

    def set_actuator_source(self, source: ActuatorSource) -> None:
        """
        Define uma função que fornece os comandos de atuadores
        a cada passo de simulação.

        A função deve retornar uma tupla:
            (o_aceleracao: float, o_direcao: float)

        Exemplos de fonte:
            - cliente MQTT que lê atr/truck/<id>/atuadores;
            - interface local que mantém sliders/botões;
            - gerador interno (piloto automático).
        """
        self._actuator_source = source
    # ----------------------------------------------------------------------
    # Atuadores
    # ----------------------------------------------------------------------

    def set_actuators(self, aceleracao: float, direcao: float) -> None:
        """
        Define comandos de atuadores no estado interno.

        Parâmetros:
            aceleracao : comando o_aceleracao [-100, 100] %
            direcao    : comando o_direcao    [-180, 180] graus
        """
        self.state.o_aceleracao = aceleracao
        self.state.o_direcao = direcao

    # ----------------------------------------------------------------------
    # Gerador interno de atuadores (perfil de teste)
    # ----------------------------------------------------------------------

    def _generate_internal_actuators(self) -> Tuple[float, float]:
        """
        Gera um padrão simples de atuadores para testar a dinâmica:

        - o_aceleracao: valor positivo constante (anda para frente).
        - o_direcao   : senoide em função do tempo de simulação, gerando zig-zag.
        """
        # Aceleração constante
        base_accel = 30.0

        # Zig-zag: direção em senoide com período de ~20 s e amplitude de 20 graus
        period_s = 20.0
        omega = 2.0 * math.pi / period_s
        steering_amp = 20.0

        steering = steering_amp * math.sin(omega * self.sim_time)

        return base_accel, steering
    # ----------------------------------------------------------------------
    # Passo de simulação
    # ----------------------------------------------------------------------

    def step(self, dt: Optional[float] = None) -> None:
        """
        Executa um passo de simulação.

        Fluxo:
            1) Se o simulador não estiver rodando, retorna sem efeito.
            2) Usa dt especificado ou o dt_default da configuração.
            3) Gera comandos internos de atuadores (perfil de teste).
            4) Publica esses comandos via callbacks de atuadores (ex.: MQTT).
            5) Lê comandos da fonte externa (ex.: Ack via tópico MQTT).
            6) Atualiza a dinâmica física via update_dynamics().
            7) Atualiza o tempo interno de simulação (sim_time).
            8) Gera dicionário de sensores com ruído.
            9) Invoca todos os callbacks registrados.

        Esta função deve ser chamada periodicamente pela UI (ex.: QTimer)
        ou por um laço externo (testes CLI, scripts de publisher, etc.).
        """
        if not self._running:
            return

        step_dt = dt if dt is not None else self.config.dt_default
        if step_dt <= 0.0:
            return


        # 2) Lê o estado atual dos atuadores da fonte externa (ex.: MQTT subscriber)
        #    Isso permite fechar o loop: o que foi publicado no tópico de atuadores
        #    é lido de volta via ActuatorMqttClient e usado na dinâmica.
        if self._actuator_source is not None:
            o_acel_ext, o_dir_ext = self._actuator_source()
            self.set_actuators(o_acel_ext, o_dir_ext)
        else:
            o_acel_cmd, o_dir_cmd = self._generate_internal_actuators()
            self.set_actuators(o_acel_cmd, o_dir_cmd)

        # 3) Atualiza dinâmica física (posição, ângulo, temperatura)
        update_dynamics(self.state, step_dt, ambient=self.config.ambient_temp)

        # 4) Avança tempo de simulação
        self.sim_time += step_dt

        # 5) Gera mapa de sensores com ruído
        sensors_noisy = self._compute_noisy_sensors_dict()

        # 6) Notifica observadores
        for cb in list(self._callbacks):
            cb(self.state, sensors_noisy)

    # ----------------------------------------------------------------------
    # Geração de sensores com ruído
    # ----------------------------------------------------------------------

    def _compute_noisy_sensors_dict(self) -> Dict[str, Any]:
        """
        Gera um dicionário com os sensores definidos na Tabela 1,
        adicionando ruído gaussiano de média nula às grandezas contínuas.

        Sensores:
            - i_posicao_x
            - i_posicao_y
            - i_angulo_x
            - i_temperatura
            - i_falha_eletrica
            - i_falha_hidraulica
        """
        # Sensores "ideais" sem ruído
        base = self.state.get_sensor_dict()

        result: Dict[str, Any] = {}

        for name, value in base.items():
            cfg = self._noise_configs.get(name)

            # Aplica ruído apenas em grandezas numéricas com configuração de ruído
            if isinstance(value, (int, float)) and cfg is not None:
                noisy_val = apply_noise(float(value), cfg)
                result[name] = noisy_val
            else:
                # Flags de falha (bool) e outros campos preservam valor exato
                result[name] = value
        return result

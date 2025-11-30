"""
Equações de diferenças para a dinâmica do caminhão na Simulação da Mina.

Atende ao requisito do documento "2025_2 - ATR - Trabalho Final.pdf" de
gerar valores de posição a partir dos atuadores de aceleração e direção,
usando uma equação diferencial (ou de diferenças) para simular a dinâmica
(inércia) de movimentação do caminhão.:contentReference[oaicite:7]{index=7}

Interface pública:
    - update_position(state, dt)
    - update_angle(state, dt)
    - update_temperature(state, dt)
    - update_dynamics(state, dt, ambient)  # função de conveniência

Onde:
    - state  : instancia de TruckState (model.truck_state)
    - dt     : passo de tempo em segundos
    - ambient: temperatura ambiente de referência [°C]
"""

from __future__ import annotations

import math
from typing import Final

from .truck_state import TruckState

# ---------------------------------------------------------------------------
# Parâmetros físicos da simulação (ajustáveis pelo grupo)
# ---------------------------------------------------------------------------

# Aceleração máxima física [m/s²] correspondente a o_aceleracao = ±100%.
# Não é especificada no ATR; valor escolhido para dar dinâmica razoável. [referência necessária]
MAX_ACCEL_M_S2: Final[float] = 5.0

# Velocidade máxima em módulo [m/s] (~54 km/h). [referência necessária]
MAX_SPEED_M_S: Final[float] = 15.0

# Coeficiente de atrito/arrasto (1/s). Simula resistência do ar/rolagem,
# fazendo o veículo desacelerar quando o acelerador é solto. [referência necessária]
FRICTION_COEFF: Final[float] = 0.5

# Zona morta de velocidade [m/s] e de comando de aceleração [%] para "parar" o veículo.
SPEED_DEADBAND: Final[float] = 0.05
THROTTLE_DEADBAND: Final[float] = 0.05

# Taxa máxima de rotação [graus/s] para comando de direção extremo o_direcao = ±180. [referência necessária]
MAX_STEER_RATE_DEG_S: Final[float] = 45.0

# Temperatura ambiente [°C]. O documento define faixa [-100, 200] e limiares de
# alerta/defeito, mas não a dinâmica térmica; adotamos 25 °C como referência.:contentReference[oaicite:8]{index=8}
AMBIENT_TEMP_C: Final[float] = 25.0

# Coeficientes térmicos: aquecimento por esforço e resfriamento passivo.
# São parâmetros de simulação, não do documento. [referência necessária]
HEATING_RATE: Final[float] = 10.0   # °C/s a 100% de esforço
COOLING_RATE: Final[float] = 0.2    # 1/s (fator de decaimento térmico)


# ---------------------------------------------------------------------------
# Funções auxiliares
# ---------------------------------------------------------------------------

def _clamp(value: float, vmin: float, vmax: float) -> float:
    """Restringe value ao intervalo [vmin, vmax]."""
    return max(vmin, min(value, vmax))


def _wrap_angle_deg(angle_deg: float) -> float:
    """
    Normaliza um ângulo em graus para o intervalo [0, 360).

    Mantém o ângulo numericamente estável ao longo da simulação e facilita
    exibição em UI.
    """
    return angle_deg % 360.0


# ---------------------------------------------------------------------------
# Dinâmica de posição (velocidade + posição X/Y)
# ---------------------------------------------------------------------------

def update_position(state: TruckState, dt: float) -> None:
    """
    Atualiza a velocidade linear e a posição (i_posicao_x, i_posicao_y).

    Lógica (equações de diferenças):
        1. Converte o comando o_aceleracao [-100, 100] em throttle [-1, 1].
        2. Calcula a aceleração comandada: a_cmd = throttle * MAX_ACCEL_M_S2.
        3. Calcula o termo de arrasto proporcional à velocidade.
        4. Aceleração resultante: a_res = a_cmd - friction * v.
        5. Integra velocidade: v_{k+1} = v_k + a_res * dt.
        6. Aplica "zona morta" para velocidades muito pequenas.
        7. Atualiza posição em 2D com integração de Euler:
               x_{k+1} = x_k + v * cos(theta) * dt
               y_{k+1} = y_k + v * sin(theta) * dt

    Parâmetros:
        state: TruckState a ser atualizado (in-place).
        dt   : passo de tempo em segundos.
    """
    # 1) Comando de aceleração normalizado
    throttle = _clamp(state.o_aceleracao, -100.0, 100.0) / 100.0

    # 2) Aceleração comandada
    accel_cmd = throttle * MAX_ACCEL_M_S2

    # 3) Termo de arrasto proporcional à velocidade
    drag = state.velocidade * FRICTION_COEFF

    # 4) Aceleração resultante
    accel_net = accel_cmd - drag

    # 5) Integração da velocidade
    state.velocidade += accel_net * dt
    state.velocidade = _clamp(state.velocidade, -MAX_SPEED_M_S, MAX_SPEED_M_S)

    # 6) Zona morta para parar o veículo quando muito lento e sem comando
    if abs(state.velocidade) < SPEED_DEADBAND and abs(throttle) < THROTTLE_DEADBAND:
        state.velocidade = 0.0

    # 7) Atualização da posição (cinemática 2D)
    theta_rad = math.radians(state.i_angulo_x)
    dx = state.velocidade * math.cos(theta_rad) * dt
    dy = state.velocidade * math.sin(theta_rad) * dt

    state.i_posicao_x += dx
    state.i_posicao_y += dy


# ---------------------------------------------------------------------------
# Dinâmica de ângulo (rotação do veículo)
# ---------------------------------------------------------------------------

def update_angle(state: TruckState, dt: float) -> None:
    """
    Atualiza o ângulo i_angulo_x [graus] e a velocidade_angular.

    Lógica:
        1. Normaliza o comando o_direcao [-180, 180] para [-1, 1].
        2. Converte em taxa de giro em graus/s, proporcional a MAX_STEER_RATE_DEG_S.
        3. Integra o ângulo: ang_{k+1} = ang_k + rate * dt.
        4. Normaliza o ângulo em [0, 360).
        5. Atualiza velocidade_angular em rad/s (derivada aproximada do ângulo).

    Parâmetros:
        state: TruckState a ser atualizado.
        dt   : passo de tempo em segundos.
    """
    # 1) Comando de direção normalizado
    steer_norm = _clamp(state.o_direcao, -180.0, 180.0) / 180.0

    # 2) Taxa de variação de ângulo (graus/s)
    steer_rate_deg_s = steer_norm * MAX_STEER_RATE_DEG_S

    # 3) Integração do ângulo
    new_angle = state.i_angulo_x + steer_rate_deg_s * dt

    # 4) Normalização
    state.i_angulo_x = _wrap_angle_deg(new_angle)

    # 5) Velocidade angular aproximada em rad/s
    state.velocidade_angular = math.radians(steer_rate_deg_s)


# ---------------------------------------------------------------------------
# Dinâmica térmica (aquecimento/resfriamento do motor)
# ---------------------------------------------------------------------------

def update_temperature(
    state: TruckState,
    dt: float,
    ambient: float = AMBIENT_TEMP_C,
) -> None:
    """
    Atualiza a temperatura i_temperatura [°C] do caminhão.

    Modelo simples:
        effort     = |o_aceleracao| / 100
        heating    = HEATING_RATE * effort * dt
        delta_temp = T_k - ambient
        cooling    = COOLING_RATE * delta_temp * dt
        T_{k+1}    = T_k + heating - cooling

    A temperatura é uma das variáveis monitoradas na Tabela 1 e usada
    pela lógica de falhas (limiares de alerta e defeito).:contentReference[oaicite:9]{index=9}

    Parâmetros:
        state  : TruckState a ser atualizado.
        dt     : passo de tempo em segundos.
        ambient: temperatura ambiente de referência [°C].
    """
    # Esforço é o módulo da aceleração solicitada (0.0 a 1.0)
    effort = abs(_clamp(state.o_aceleracao, -100.0, 100.0)) / 100.0

    # Aquecimento por esforço
    heating = HEATING_RATE * effort * dt

    # Resfriamento proporcional ao excesso de temperatura
    delta_temp = state.i_temperatura - ambient
    cooling = COOLING_RATE * delta_temp * dt

    # Equação de diferenças da temperatura
    state.i_temperatura += heating - cooling


# ---------------------------------------------------------------------------
# Função de alto nível para o Simulator
# ---------------------------------------------------------------------------

def update_dynamics(
    state: TruckState,
    dt: float,
    ambient: float = AMBIENT_TEMP_C,
) -> None:
    """
    Atualiza, em um único passo, ângulo, posição e temperatura do caminhão.

    Conveniência para o core.Simulator:
        - Atualiza primeiro o ângulo (steering),
        - depois a posição (cinemática 2D),
        - e por fim a temperatura.

    Parâmetros:
        state  : TruckState a ser atualizado.
        dt     : passo de tempo em segundos.
        ambient: temperatura ambiente de referência [°C].
    """
    update_angle(state, dt)
    update_position(state, dt)
    update_temperature(state, dt, ambient=ambient)

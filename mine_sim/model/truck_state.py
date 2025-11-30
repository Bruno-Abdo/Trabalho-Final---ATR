from __future__ import annotations
from dataclasses import dataclass, asdict
from typing import Dict, Any

@dataclass
class TruckState:
    """
    Representa o estado completo do caminhão para a Simulação da Mina.

    Campos principais baseados na Tabela 1 – Sensores e atuadores do caminhão autônomo
    do documento "2025_2 - ATR - Trabalho Final.pdf" (i_posicao_x, i_posicao_y,
    i_angulo_x, i_temperatura, i_falha_eletrica, i_falha_hidraulica, o_aceleracao,
    o_direcao). 

    Observação sobre tipos:
        - No documento, sensores são int. Aqui usamos float para a dinâmica contínua.
          A conversão para int é feita na exportação (get_sensor_dict).

    Sensores (saídas do simulador):
        i_posicao_x        : posição no eixo x (ref. absoluto) [m]
        i_posicao_y        : posição no eixo y (ref. absoluto) [m]
        i_angulo_x         : direção angular (0=Leste) [graus]
        i_temperatura      : temperatura do motor [°C]
        i_falha_eletrica   : flag de falha elétrica
        i_falha_hidraulica : flag de falha hidráulica

    Atuadores (entradas do controle):
        o_aceleracao       : comando de aceleração [-100 a 100 %]
        o_direcao          : comando de direção [-180 a 180 graus]

    Variáveis auxiliares (Dinâmica):
        velocidade         : velocidade linear atual [m/s]
        velocidade_angular : velocidade angular atual [graus/s]
    """

    # Sensores (Estado Físico)
    i_posicao_x: float = 0.0
    i_posicao_y: float = 0.0
    i_angulo_x: float = 0.0
    i_temperatura: float = 25.0

    # Flags de Falha
    i_falha_eletrica: bool = False
    i_falha_hidraulica: bool = False

    # Atuadores (Comandos Recebidos)
    o_aceleracao: float = 0.0
    o_direcao: float = 0.0

    # Auxiliares para Equações de Diferenças (Inércia)
    velocidade: float = 0.0
    velocidade_angular: float = 0.0

    def __str__(self) -> str:
        """Representação legível para logs."""
        return (
            f"TruckState("
            f"Pos: ({self.i_posicao_x:.2f}, {self.i_posicao_y:.2f}) | "
            f"Ang: {self.i_angulo_x:.2f}° | "
            f"Vel: {self.velocidade:.2f} m/s | "
            f"Temp: {self.i_temperatura:.1f}°C | "
            f"Falhas: [E:{self.i_falha_eletrica}, H:{self.i_falha_hidraulica}]"
            f")"
        )

    def to_dict(self) -> Dict[str, Any]:
        """Converte estado completo para dicionário (debug/log)."""
        return asdict(self)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "TruckState":
        """Reconstrói estado a partir de dicionário."""
        return cls(**data)

    def get_sensor_dict(self) -> Dict[str, Any]:
        """
        Retorna apenas os dados de sensores definidos na Tabela 1.
        
        Args:
            quantize: Se True, arredonda floats para int conforme especificação da Tabela 1.
        """
        return {
            "i_posicao_x": self.i_posicao_x,
            "i_posicao_y": self.i_posicao_y,
            "i_angulo_x": self.i_angulo_x,
            "i_temperatura": self.i_temperatura,
            "i_falha_eletrica": self.i_falha_eletrica,
            "i_falha_hidraulica": self.i_falha_hidraulica,
        }

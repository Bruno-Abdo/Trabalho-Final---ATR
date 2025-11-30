"""
Geração de ruído aleatório de média nula para sensores da Simulação da Mina.

Implementa ruído gaussiano para simular imperfeições reais de medição em
sensores físicos (posição, ângulo, temperatura).

Referência:
    "2025_2 - ATR - Trabalho Final.pdf" - Especificações Gerais:
    "Os dados gerados dos sensores devem somar um ruído aleatório, de média nula".
"""

from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Final, Dict

# ===========================================================================
# CONFIGURAÇÃO DE RUÍDO PADRÃO
# ===========================================================================

# Valores padrão razoáveis para a escala da mina e do caminhão.
# Posição: ~10cm de erro; Ângulo: ~0.5 grau; Temp: ~0.5 grau.
DEFAULT_NOISE_POSITION: Final[float] = 0.1      # [m]
DEFAULT_NOISE_ANGLE: Final[float] = 0.5         # [graus]
DEFAULT_NOISE_TEMPERATURE: Final[float] = 0.5   # [°C]


@dataclass
class NoiseConfig:
    """
    Configuração de ruído para um sensor específico.
    Utilizada para passar parâmetros entre a UI e o Simulador.
    """
    std: float = 0.1        # Desvio padrão (intensidade)
    enabled: bool = True    # Se False, o ruído não é aplicado


# ===========================================================================
# FUNÇÕES GERADORAS
# ===========================================================================

def gaussian_noise(std: float) -> float:
    """
    Gera uma amostra de ruído gaussiano com média nula (mu=0.0).
    Requisito estrito do documento ATR.
    """
    if std <= 0.0:
        return 0.0
    return random.gauss(mu=0.0, sigma=std)


def apply_noise(value: float, config: NoiseConfig) -> float:
    """
    Aplica ruído aditivo a um valor base, se habilitado na configuração.
    Retorna: value + N(0, std²)
    """
    if not config.enabled or config.std <= 0.0:
        return value
    return value + gaussian_noise(config.std)

# ===========================================================================
# HELPERS DE CONFIGURAÇÃO
# ===========================================================================

def get_default_noise_configs() -> Dict[str, NoiseConfig]:
    """
    Retorna o mapa inicial de configurações de ruído para os sensores conhecidos.
    Útil para inicializar o Simulador.
    """
    return {
        'i_posicao_x': NoiseConfig(std=DEFAULT_NOISE_POSITION, enabled=True),
        'i_posicao_y': NoiseConfig(std=DEFAULT_NOISE_POSITION, enabled=True),
        'i_angulo_x': NoiseConfig(std=DEFAULT_NOISE_ANGLE, enabled=True),
        'i_temperatura': NoiseConfig(std=DEFAULT_NOISE_TEMPERATURE, enabled=True),
    }

"""
Módulo core - Núcleo da Simulação da Mina.

Contém:
    - Simulator: Loop principal de simulação (dinâmica + ruído)
    - SimulatorConfig: Configuração do simulador
    - MqttPublisher: Cliente MQTT para publicação de sensores
    - Funções de ruído: geração de ruído gaussiano de média nula
    - NoiseConfig: Configuração de ruído por sensor
"""

from .random_sources import (
    NoiseConfig,
    gaussian_noise,
    apply_noise,
    get_default_noise_configs,
    DEFAULT_NOISE_POSITION,
    DEFAULT_NOISE_ANGLE,
    DEFAULT_NOISE_TEMPERATURE,
)

from .simulator import (
    Simulator,
    SimulatorConfig,
)

from .publisher import MqttPublisher

__all__ = [
    # Configuração de ruído
    'NoiseConfig',
    
    # Funções de ruído
    'gaussian_noise',
    'apply_noise',
    'get_default_noise_configs',
    
    # Constantes de ruído
    'DEFAULT_NOISE_POSITION',
    'DEFAULT_NOISE_ANGLE',
    'DEFAULT_NOISE_TEMPERATURE',
    
    # Simulador principal
    'Simulator',
    'SimulatorConfig',
    
    # Publisher MQTT
    'MqttPublisher',
]

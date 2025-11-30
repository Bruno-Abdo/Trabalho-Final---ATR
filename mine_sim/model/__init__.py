"""
Módulo model - Modelo de dados e física da Simulação da Mina.

Contém:
    - TruckState: Estado completo do caminhão (sensores + atuadores)
    - Funções de dinâmica: equações de diferenças (posição, ângulo, temperatura)
    - Funções de injeção de falhas: controle de defeitos elétricos/hidráulicos
"""

from .truck_state import TruckState

from .dynamics import (
    update_position,
    update_angle,
    update_temperature,
    update_dynamics,
    # Constantes opcionais
    MAX_ACCEL_M_S2,
    MAX_SPEED_M_S,
    FRICTION_COEFF,
    MAX_STEER_RATE_DEG_S,
    AMBIENT_TEMP_C,
)

from .faults import (
    set_electrical_fault,
    set_hydraulic_fault,
    set_overtemperature_fault,
    clear_all_faults,
    get_fault_status,
    get_fault_description,
    TEMP_ALERT_THRESHOLD_C,
    TEMP_FAULT_THRESHOLD_C,
)

__all__ = [
    # Estado do caminhão
    'TruckState',
    
    # Funções de dinâmica
    'update_position',
    'update_angle',
    'update_temperature',
    'update_dynamics',
    
    # Constantes de dinâmica
    'MAX_ACCEL_M_S2',
    'MAX_SPEED_M_S',
    'FRICTION_COEFF',
    'MAX_STEER_RATE_DEG_S',
    'AMBIENT_TEMP_C',
    'TEMP_ALERT_THRESHOLD_C',
    'TEMP_FAULT_THRESHOLD_C',

    # Funções de falhas
    'set_electrical_fault',
    'set_hydraulic_fault',
    'set_overtemperature_fault',
    'clear_all_faults',
    'get_fault_status',
    'get_fault_description',
]

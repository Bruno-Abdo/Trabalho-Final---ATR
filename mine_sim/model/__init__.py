"""
Módulo model - Modelo de dados e física da Simulação da Mina.

Contém:
    - TruckState: Estado completo do caminhão (sensores + atuadores)
    - Funções de dinâmica: equações de diferenças (posição, ângulo, temperatura)
    - FaultInjector: Sistema de injeção de falhas para simulação
    - Funções auxiliares de injeção de falhas
"""

from .truck_state import TruckState

from .dynamics import (
    update_position,
    update_angle,
    update_temperature,
    update_dynamics,
    # Constantes físicas
    MAX_ACCEL_M_S2,
    MAX_SPEED_M_S,
    FRICTION_COEFF,
    MAX_STEER_RATE_DEG_S,
    AMBIENT_TEMP_C,
)

from .faults import (
    # Classe principal
    FaultInjector,
    
    # Funções helper (API simplificada)
    set_electrical_fault,
    set_hydraulic_fault,
    force_temperature,
    clear_all_faults,
)

# Constantes de temperatura (do documento ATR - Tabela 1)
TEMP_ALERT_THRESHOLD_C = 95   # Temperatura de alerta em °C
TEMP_FAULT_THRESHOLD_C = 120  # Temperatura de defeito em °C

__all__ = [
    # === Estado do caminhão ===
    'TruckState',
    
    # === Funções de dinâmica ===
    'update_position',
    'update_angle',
    'update_temperature',
    'update_dynamics',
    
    # === Constantes físicas de dinâmica ===
    'MAX_ACCEL_M_S2',
    'MAX_SPEED_M_S',
    'FRICTION_COEFF',
    'MAX_STEER_RATE_DEG_S',
    'AMBIENT_TEMP_C',
    
    # === Sistema de injeção de falhas ===
    'FaultInjector',
    
    # === Funções helper de falhas ===
    'set_electrical_fault',
    'set_hydraulic_fault',
    'force_temperature',
    'clear_all_faults',
    
    # === Constantes de temperatura ===
    'TEMP_ALERT_THRESHOLD_C',
    'TEMP_FAULT_THRESHOLD_C',
]


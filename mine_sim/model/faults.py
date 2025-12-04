"""
model/faults.py

Sistema de injeção de falhas para simulação do caminhão autônomo.

Funcionalidades:
- Injeção de falha elétrica
- Injeção de falha hidráulica
- Forçar temperatura crítica (>120°C)
- Forçar temperatura de alerta (>95°C)
- Reset de todas as falhas
"""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from model.truck_state import TruckState

logger = logging.getLogger(__name__)


class FaultInjector:
    """
    Gerenciador de injeção de falhas no sistema.
    
    Mantém estado persistente das falhas ativas e aplica-as
    ao TruckState antes de cada passo de simulação.
    """
    
    def __init__(self):
        # Estado persistente das falhas injetadas
        self._electric_fault_active = False
        self._hydraulic_fault_active = False
        self._force_overheat = False  # >120°C (defeito)
        self._force_warning = False   # >95°C (alerta)
        
        # Temperaturas forçadas
        self._overheat_temperature = 125.0  # °C
        self._warning_temperature = 100.0   # °C
    
    # ========================================================================
    # Controle de Falhas Elétricas
    # ========================================================================
    
    def set_electric_fault(self, active: bool) -> None:
        """
        Ativa ou desativa falha elétrica.
        
        Args:
            active: True para ativar falha, False para desativar
        """
        self._electric_fault_active = active
        logger.info(
            "Falha elétrica %s",
            "ATIVADA" if active else "DESATIVADA"
        )
    
    def toggle_electric_fault(self) -> bool:
        """
        Alterna estado da falha elétrica.
        
        Returns:
            Novo estado (True = ativa, False = inativa)
        """
        self._electric_fault_active = not self._electric_fault_active
        logger.info(
            "Falha elétrica %s",
            "ATIVADA" if self._electric_fault_active else "DESATIVADA"
        )
        return self._electric_fault_active
    
    def is_electric_fault_active(self) -> bool:
        """Retorna True se falha elétrica está ativa."""
        return self._electric_fault_active
    
    # ========================================================================
    # Controle de Falhas Hidráulicas
    # ========================================================================
    
    def set_hydraulic_fault(self, active: bool) -> None:
        """
        Ativa ou desativa falha hidráulica.
        
        Args:
            active: True para ativar falha, False para desativar
        """
        self._hydraulic_fault_active = active
        logger.info(
            "Falha hidráulica %s",
            "ATIVADA" if active else "DESATIVADA"
        )
    
    def toggle_hydraulic_fault(self) -> bool:
        """
        Alterna estado da falha hidráulica.
        
        Returns:
            Novo estado (True = ativa, False = inativa)
        """
        self._hydraulic_fault_active = not self._hydraulic_fault_active
        logger.info(
            "Falha hidráulica %s",
            "ATIVADA" if self._hydraulic_fault_active else "DESATIVADA"
        )
        return self._hydraulic_fault_active
    
    def is_hydraulic_fault_active(self) -> bool:
        """Retorna True se falha hidráulica está ativa."""
        return self._hydraulic_fault_active
    
    # ========================================================================
    # Controle de Temperatura
    # ========================================================================
    
    def set_temperature_overheat(self, active: bool, temperature: float = 125.0) -> None:
        """
        Força temperatura de defeito (>120°C).
        
        Args:
            active: True para forçar superaquecimento
            temperature: Temperatura alvo em °C (padrão: 125°C)
        """
        self._force_overheat = active
        if active:
            self._overheat_temperature = max(120.0, temperature)
            # Desativa alerta se overheat ativo
            self._force_warning = False
            logger.warning(
                "SUPERAQUECIMENTO forçado: %.1f°C",
                self._overheat_temperature
            )
        else:
            logger.info("Superaquecimento forçado DESATIVADO")
    
    def set_temperature_warning(self, active: bool, temperature: float = 100.0) -> None:
        """
        Força temperatura de alerta (95-119°C).
        
        Args:
            active: True para forçar alerta
            temperature: Temperatura alvo em °C (padrão: 100°C)
        """
        self._force_warning = active
        if active:
            self._warning_temperature = max(95.0, min(119.0, temperature))
            # Desativa overheat se warning ativo
            self._force_overheat = False
            logger.warning(
                "Temperatura de ALERTA forçada: %.1f°C",
                self._warning_temperature
            )
        else:
            logger.info("Temperatura de alerta forçada DESATIVADA")
    
    def is_temperature_forced(self) -> bool:
        """Retorna True se alguma temperatura está sendo forçada."""
        return self._force_overheat or self._force_warning
    
    # ========================================================================
    # Reset e Aplicação
    # ========================================================================
    
    def reset_all_faults(self) -> None:
        """
        Reseta todas as falhas para estado normal.
        """
        self._electric_fault_active = False
        self._hydraulic_fault_active = False
        self._force_overheat = False
        self._force_warning = False
        logger.info("🔄 Todas as falhas foram RESETADAS")
    
    def apply_faults(self, state: TruckState) -> None:
        """
        Aplica todas as falhas ativas ao TruckState.
        
        IMPORTANTE: Este método SEMPRE define o estado completo das falhas,
        tanto ativando quanto DESATIVANDO conforme necessário.
        
        Args:
            state: TruckState a ser modificado
        """
        # === SEMPRE DEFINE O ESTADO (CORRIGE O BUG) ===
        
        # Define falha elétrica (ativa OU desativa)
        state.i_falha_eletrica = self._electric_fault_active
        
        # Define falha hidráulica (ativa OU desativa)
        state.i_falha_hidraulica = self._hydraulic_fault_active
        
        # Força temperatura se necessário
        # (Não resetamos temperatura aqui pois ela evolui naturalmente)
        if self._force_overheat:
            state.i_temperatura = self._overheat_temperature
        elif self._force_warning:
            state.i_temperatura = self._warning_temperature
        # Se nenhuma temperatura forçada, deixa evoluir naturalmente
    
    # ========================================================================
    # Estado e Serialização
    # ========================================================================
    
    def get_fault_summary(self) -> dict:
        """
        Retorna sumário do estado atual das falhas.
        
        Returns:
            Dicionário com status de todas as falhas
        """
        return {
            'electric_fault': self._electric_fault_active,
            'hydraulic_fault': self._hydraulic_fault_active,
            'temperature_overheat': self._force_overheat,
            'temperature_warning': self._force_warning,
            'any_fault_active': self.has_any_fault_active()
        }
    
    def has_any_fault_active(self) -> bool:
        """Retorna True se alguma falha está ativa."""
        return (
            self._electric_fault_active or
            self._hydraulic_fault_active or
            self._force_overheat or
            self._force_warning
        )


# ============================================================================
# Funções Helper (API simplificada)
# ============================================================================

def set_electrical_fault(state: TruckState, enabled: bool) -> None:
    """
    API simplificada: Define falha elétrica diretamente no estado.
    
    Args:
        state: TruckState a modificar
        enabled: True para ativar, False para desativar
    """
    state.i_falha_eletrica = enabled
    logger.info("Falha elétrica %s", "ativada" if enabled else "desativada")


def set_hydraulic_fault(state: TruckState, enabled: bool) -> None:
    """
    API simplificada: Define falha hidráulica diretamente no estado.
    
    Args:
        state: TruckState a modificar
        enabled: True para ativar, False para desativar
    """
    state.i_falha_hidraulica = enabled
    logger.info("Falha hidráulica %s", "ativada" if enabled else "desativada")


def force_temperature(state: TruckState, temperature: float) -> None:
    """
    API simplificada: Força temperatura específica.
    
    Args:
        state: TruckState a modificar
        temperature: Temperatura desejada em °C
    """
    state.i_temperatura = temperature
    logger.info("Temperatura forçada para %.1f°C", temperature)


def clear_all_faults(state: TruckState) -> None:
    """
    API simplificada: Limpa todas as falhas.
    
    Args:
        state: TruckState a modificar
    """
    state.i_falha_eletrica = False
    state.i_falha_hidraulica = False
    # Não reseta temperatura (ela deve evoluir naturalmente)
    logger.info("Todas as falhas limpas")


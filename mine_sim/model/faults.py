"""
Lógica de injeção de falhas na Simulação da Mina.

Este módulo implementa funções para alterar o estado do TruckState de forma
a simular condições de erro no caminhão, atendendo ao requisito de permitir
que a tarefa de Simulação da Mina "gere defeito em algum caminhão" na
interface de simulação.

Baseado em:
    - Tabela 1 (página 3, seção 4 – Especificações Gerais do Sistema) do
      documento "2025_2 - ATR - Trabalho Final.pdf", que define:
        * i_temperatura com faixa [-100, 200] e limiares:
              alerta se T > 95 °C,
              defeito se T > 120 °C,
        * i_falha_eletrica e i_falha_hidraulica como flags de falha.
"""

from __future__ import annotations

from typing import Final, Dict

from .truck_state import TruckState

# ---------------------------------------------------------------------------
# Limiares de temperatura (diretamente da Tabela 1)
# ---------------------------------------------------------------------------

# Nível de alerta: T > 95 °C (Tabela 1).
TEMP_ALERT_THRESHOLD_C: Final[float] = 95.0 

# Nível de defeito: T > 120 °C (Tabela 1).
TEMP_FAULT_THRESHOLD_C: Final[float] = 120.0

# ---------------------------------------------------------------------------
# Parâmetros de simulação (não definidos no documento) [referência necessária]
# ---------------------------------------------------------------------------

# Temperatura usada quando o usuário força um defeito de overtemperature.
# Deve ser estritamente maior que TEMP_FAULT_THRESHOLD_C.
TEMP_FORCED_OVERHEAT_C: Final[float] = 130.0  # [referência necessária]

# Temperatura "segura" para retornar após remover falha térmica.
# Escolhida abaixo do limiar de alerta.
TEMP_SAFE_OPERATION_C: Final[float] = 85.0  # [referência necessária]


# ---------------------------------------------------------------------------
# Funções de injeção de falha (chamadas pela UI ou pelo Simulator)
# ---------------------------------------------------------------------------

def set_electrical_fault(state: TruckState, enabled: bool) -> None:
    """
    Ativa ou desativa a falha elétrica (i_falha_eletrica).

    O sensor i_falha_eletrica indica presença de falha no sistema elétrico
    do veículo (Tabela 1).
    """
    state.i_falha_eletrica = bool(enabled)


def set_hydraulic_fault(state: TruckState, enabled: bool) -> None:
    """
    Ativa ou desativa a falha hidráulica (i_falha_hidraulica).

    O sensor i_falha_hidraulica indica presença de falha no sistema
    hidráulico do veículo (Tabela 1).
    """
    state.i_falha_hidraulica = bool(enabled)


def set_overtemperature_fault(
    state: TruckState,
    enabled: bool,
    *,
    critical: bool = True,
) -> None:
    """
    Simula uma condição de sobretemperatura do motor (i_temperatura).

    A Tabela 1 define:
        - nível de alerta se T > 95 °C,
        - defeito se T > 120 °C.

    Parâmetros:
        state   : TruckState a ser modificado.
        enabled : Se True, força uma temperatura acima do limiar selecionado.
                  Se False, retorna a temperatura para TEMP_SAFE_OPERATION_C
                  apenas se estiver acima do nível de alerta.
        critical: Se True, força temperatura em nível de defeito (> 120 °C).
                  Se False, poderia ser usado para simular apenas nível de
                  alerta (> 95 °C). No momento, a implementação força sempre
                  valor em nível de defeito, e esse parâmetro está reservado
                  para extensões futuras. [referência necessária]
    """
    if enabled:
        # Força condição de defeito (superaquecimento) para garantir detecção
        # pela tarefa de Monitoramento de Falhas.
        state.i_temperatura = TEMP_FORCED_OVERHEAT_C
    else:
        # Só ajusta se ainda estiver acima do alerta, para não interferir
        # na dinâmica térmica normal quando já está fria.
        if state.i_temperatura > TEMP_ALERT_THRESHOLD_C:
            state.i_temperatura = TEMP_SAFE_OPERATION_C


def clear_all_faults(state: TruckState) -> None:
    """
    Remove falhas elétricas/hidráulicas injetadas e normaliza a temperatura
    caso esteja acima do nível de alerta.

    Útil para a UI implementar um botão de "Reset de Falhas".
    """
    state.i_falha_eletrica = False
    state.i_falha_hidraulica = False

    if state.i_temperatura > TEMP_ALERT_THRESHOLD_C:
        state.i_temperatura = TEMP_SAFE_OPERATION_C


# ---------------------------------------------------------------------------
# Funções de consulta/diagnóstico (apoio à UI e logs)
# ---------------------------------------------------------------------------

def get_fault_status(state: TruckState) -> Dict[str, bool]:
    """
    Retorna um dicionário com o status das falhas, para ligação simples com a UI.

    Chaves:
        - "electrical"     : True se i_falha_eletrica == True.
        - "hydraulic"      : True se i_falha_hidraulica == True.
        - "overtemperature": True se i_temperatura > TEMP_FAULT_THRESHOLD_C.
    """
    return {
        "electrical": state.i_falha_eletrica,
        "hydraulic": state.i_falha_hidraulica,
        "overtemperature": state.i_temperatura > TEMP_FAULT_THRESHOLD_C,
    }


def get_fault_description(state: TruckState) -> str:
    """
    Gera uma string descrevendo as falhas ativas, útil para logs/dashboards.

    Exemplos:
        - "NORMAL"
        - "ELÉTRICA"
        - "ELÉTRICA | HIDRÁULICA | SUPERAQUECIMENTO (130°C)"
        - "ALERTA TEMP (100°C)"
    """
    active_faults = []

    if state.i_falha_eletrica:
        active_faults.append("ELÉTRICA")

    if state.i_falha_hidraulica:
        active_faults.append("HIDRÁULICA")

    if state.i_temperatura > TEMP_FAULT_THRESHOLD_C:
        active_faults.append(f"SUPERAQUECIMENTO ({state.i_temperatura:.0f}°C)")
    elif state.i_temperatura > TEMP_ALERT_THRESHOLD_C:
        active_faults.append(f"ALERTA TEMP ({state.i_temperatura:.0f}°C)")

    if not active_faults:
        return "NORMAL"

    return " | ".join(active_faults)

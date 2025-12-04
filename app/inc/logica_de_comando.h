#ifndef LOGICA_DE_COMANDO_H
#define LOGICA_DE_COMANDO_H

#include <atomic>
#include <boost/thread.hpp>
#include "config.hpp"
#include "buffer_circular_compartilhado.h"
#include "controle_de_navegacao.h" // NavigationControlState, LogicSharedState
#include "evento_de_falhas.h"      // FaultEventBus, FaultEvent

/**
 * @brief Thread de Lógica de Comando (Árbitro Central + Interface Local).
 *
 * Esta versão implementa:
 * - Subscriber MQTT para comandos da Interface Local (modo + comandos manuais)
 * - Processamento de eventos de falha do FaultEventBus
 * - Arbitragem entre comandos automáticos (Controle de Navegação) e manuais
 * - Publicação de comandos finais no MQTT com telemetria rica
 *
 * @param id             Identificador lógico da tarefa (ex: ID_COMANDO)
 * @param sleep_ms       Período da tarefa em milissegundos (ex: SLEEP_MS_COMANDO)
 * @param running_flag   Flag global de execução (false => shutdown)
 * @param buffer         Buffer circular (reservado para uso futuro)
 * @param event_bus      Barramento de eventos de falha - LEITURA
 * @param nav_state      Comandos automáticos do Controle de Navegação - LEITURA
 * @param logic_state    Estado de operação (automático/manual, defeito) - ESCRITA
 */
void comando_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer,
                    FaultEventBus &event_bus,
                    NavigationControlState &nav_state,
                    LogicSharedState &logic_state);

#endif // LOGICA_DE_COMANDO_H

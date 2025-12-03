#ifndef COLETOR_DE_DADOS_H
#define COLETOR_DE_DADOS_H

#include <atomic>
#include "buffer_circular_compartilhado.h"
#include "evento_de_falhas.h"

/**
 * @brief Thread responsável por persistir dados e eventos em arquivos CSV.
 * * @param id ID da thread
 * @param sleep_ms Tempo de espera entre ciclos de coleta
 * @param running_flag Flag de controle de execução
 * @param buffer Referência ao buffer de dados (telemetria)
 * @param event_bus Referência ao barramento de eventos (falhas)
 */
void coletor_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer,
                    FaultEventBus &event_bus); // Novo parâmetro

#endif // COLETOR_DE_DADOS_H

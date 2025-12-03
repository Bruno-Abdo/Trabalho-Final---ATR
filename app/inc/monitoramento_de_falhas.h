#ifndef MONITORAMENTO_DE_FALHAS_H
#define MONITORAMENTO_DE_FALHAS_H

#include <string>
#include <atomic>
#include "evento_de_falhas.h"

void monitoramento_thread(int id,
                          int sleep_ms,
                          std::atomic<bool> &running_flag,
                          FaultEventBus &event_bus);

#endif // MONITORAMENTO_DE_FALHAS_H
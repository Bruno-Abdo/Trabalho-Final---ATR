#ifndef MONITORAMENTO_DE_FALHAS_H
#define MONITORAMENTO_DE_FALHAS_H

#include <string>
#include <atomic>
#include "buffer_circular_compartilhado.h"

void monitoramento_thread(int id,
                          int sleep_ms,
                          std::atomic<bool> &running_flag,
                          SharedCircularBuffer &buffer);

#endif // MONITORAMENTO_DE_FALHAS_H
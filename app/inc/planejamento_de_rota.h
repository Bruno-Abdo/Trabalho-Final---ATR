#ifndef PLANEJAMENTO_DE_ROTA_H
#define PLANEJAMENTO_DE_ROTA_H

#include <string>
#include <atomic>
#include "buffer_circular_compartilhado.h"

void planejamento_thread(int id,
                         int sleep_ms,
                         std::atomic<bool> &running_flag,
                         SharedCircularBuffer &buffer);

#endif // PLANEJAMENTO_DE_ROTA_H
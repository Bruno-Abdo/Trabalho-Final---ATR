#ifndef TRATAMENTO_SENSORES_H
#define TRATAMENTO_SENSORES_H

#include <string>
#include <atomic>
#include "buffer_circular_compartilhado.h"

void tratamento_thread(int id,
                       int sleep_ms,
                       std::atomic<bool> &running_flag,
                       SharedCircularBuffer &buffer);

#endif // TRATAMENTO_SENSORES_H
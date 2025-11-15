#ifndef LOGICA_DE_COMANDO_H
#define LOGICA_DE_COMANDO_H

#include <string>
#include <atomic>
#include "buffer_circular_compartilhado.h"

void comando_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer);

#endif // LOGICA_DE_COMANDO_H
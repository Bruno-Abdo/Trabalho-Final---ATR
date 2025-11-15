#ifndef CONTROLE_DE_NAVEGACAO_H
#define CONTROLE_DE_NAVEGACAO_H

#include <string>
#include <atomic>
#include "buffer_circular_compartilhado.h"

void controle_thread(int id,
                     int sleep_ms,
                     std::atomic<bool> &running_flag,
                     SharedCircularBuffer &buffer);

#endif // CONTROLE_DE_NAVEGACAO_H
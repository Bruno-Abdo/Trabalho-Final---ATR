#ifndef COLETOR_DE_DADOS_H
#define COLETOR_DE_DADOS_H

#include <string>
#include <atomic>
#include "buffer_circular_compartilhado.h"

void coletor_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer);

#endif // COLETOR_DE_DADOS_H

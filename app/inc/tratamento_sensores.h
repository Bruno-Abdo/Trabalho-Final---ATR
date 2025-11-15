#ifndef TRATAMENTO_SENSORES_H
#define TRATAMENTO_SENSORES_H

#include <string>
#include <atomic>

void tratamento_thread(const std::string &source_id,
                       int sleep_ms,
                       std::atomic<bool> &running_flag);

#endif // TRATAMENTO_SENSORES_H
#ifndef MONITORAMENTO_DE_FALHAS_H
#define MONITORAMENTO_DE_FALHAS_H

#include <string>
#include <atomic>

void monitoramento_thread(const std::string &source_id,
                          int sleep_ms,
                          std::atomic<bool> &running_flag);

#endif // MONITORAMENTO_DE_FALHAS_H
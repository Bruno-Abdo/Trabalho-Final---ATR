#ifndef PLANEJAMENTO_DE_ROTA_H
#define PLANEJAMENTO_DE_ROTA_H

#include <string>
#include <atomic>

void planejamento_thread(const std::string &source_id,
                         int sleep_ms,
                         std::atomic<bool> &running_flag);

#endif // PLANEJAMENTO_DE_ROTA_H
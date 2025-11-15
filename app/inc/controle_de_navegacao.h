#ifndef CONTROLE_DE_NAVEGACAO_H
#define CONTROLE_DE_NAVEGACAO_H

#include <string>
#include <atomic>

void controle_thread(const std::string &source_id,
                     int sleep_ms,
                     std::atomic<bool> &running_flag);

#endif // CONTROLE_DE_NAVEGACAO_H
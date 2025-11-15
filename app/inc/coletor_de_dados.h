#ifndef COLETOR_DE_DADOS_H
#define COLETOR_DE_DADOS_H

#include <string>
#include <atomic>

void coletor_thread(const std::string &source_id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag);

#endif // COLETOR_DE_DADOS_H

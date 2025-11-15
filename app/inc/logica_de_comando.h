#ifndef LOGICA_DE_COMANDO_H
#define LOGICA_DE_COMANDO_H

#include <string>
#include <atomic>

void comando_thread(const std::string &source_id,
                   int sleep_ms,
                   std::atomic<bool> &running_flag);

#endif // LOGICA_DE_COMANDO_H
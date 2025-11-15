/**
 * @file tratamento_sensores.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-11-15
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <iostream>
#include <boost/thread.hpp>
#include "tratamento_sensores.h"

void tratamento_thread(int id,
                       int sleep_ms,
                       std::atomic<bool> &running_flag,
                       SharedCircularBuffer &buffer)
{
    std::cout << "Tratamento " << id << " is starting." << std::endl;
    try
    {
        // Loop until the 'running' flag is set to false
        while (running_flag)
        {

            // This is the "work" - just printing to the log
            std::cout << "[LOG] " << "Tratamento " << id << " is running..." << std::endl;

            BufferData data;
            // ... lê sensores físicos, filtra, preenche 'data' ...
            data.i_posicao_x = 10;
            data.i_temperatura = 45;
            // ...
            // Escreve no buffer (bloqueia se estiver cheio)
            buffer.write(data, running_flag);

            // ... dorme pelo seu período ...
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        // This exception is thrown when main calls thread.interrupt()
        std::cout << "Tratamento " << id << " was interrupted." << std::endl;
    }

    std::cout << "Tratamento " << id << " is stopping." << std::endl;
}

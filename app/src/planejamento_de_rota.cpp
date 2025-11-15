/**
 * @file planejamento_de_rota.cpp
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
#include "planejamento_de_rota.h"

void planejamento_thread(int id,
                         int sleep_ms,
                         std::atomic<bool> &running_flag,
                         SharedCircularBuffer &buffer)
{
    std::cout << "Planejamento " << id << " is starting." << std::endl;
    try
    {
        // Loop until the 'running' flag is set to false
        while (running_flag)
        {
            // 1. Consome dados
            BufferData data = buffer.read(id, running_flag);
            if (!running_flag)
                break;

            // 2. Processa (lê os campos que importa)
            // Ex: Usa data.i_posicao_x para calcular a rota

            // 3. Produz (escreve de volta no buffer)
            BufferData data_out = data;          // Copia os dados
            data_out.setpoint_velocidade = 80.0; // Define seu próprio campo

            buffer.write(data_out, running_flag);

            // This is the "work" - just printing to the log
            std::cout << "[LOG] " << "Planejamento " << id << " is running..." << std::endl;

            // Sleep, but allow interruption (for clean shutdown)
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        // This exception is thrown when main calls thread.interrupt()
        std::cout << "Planejamento " << id << " was interrupted." << std::endl;
    }

    std::cout << "Planejamento " << id << " is stopping." << std::endl;
}

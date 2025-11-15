/**
 * @file logica_de_comando.cpp
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
#include "logica_de_comando.h"

void comando_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer)
{
    std::cout << "Comando " << id << " is starting." << std::endl;
    try
    {
        // Loop until the 'running' flag is set to false
        while (running_flag)
        {
            // This is the "work" - just printing to the log
            std::cout << "[LOG] " << "Comando " << id << " is running..." << std::endl;

            // Sleep, but allow interruption (for clean shutdown)
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        // This exception is thrown when main calls thread.interrupt()
        std::cout << "Comando " << id << " was interrupted." << std::endl;
    }

    std::cout << "Comando " << id << " is stopping." << std::endl;
}

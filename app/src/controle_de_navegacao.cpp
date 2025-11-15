/**
 * @file controle_de_navegacao.cpp
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
#include "controle_de_navegacao.h"

void controle_thread(int id,
                     int sleep_ms,
                     std::atomic<bool> &running_flag,
                     SharedCircularBuffer &buffer)
{
    std::cout << "Controle " << id << " is starting." << std::endl;
    try
    {
        // Loop until the 'running' flag is set to false
        while (running_flag)
        {

            // This is the "work" - just printing to the log
            std::cout << "[LOG] " << "Controle " << id << " is running..." << std::endl;

            // Sleep, but allow interruption (for clean shutdown)
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        // This exception is thrown when main calls thread.interrupt()
        std::cout << "Controle " << id << " was interrupted." << std::endl;
    }

    std::cout << "Controle " << id << " is stopping." << std::endl;
}

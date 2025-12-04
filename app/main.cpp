#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <csignal>

// Paho MQTT C++ Library
#include <mqtt/async_client.h>

// nlohmann JSON Library
#include <nlohmann/json.hpp>

// Boost Thread Library
#include <boost/thread.hpp>

// Configurations
#include "config.hpp"

#include "coletor_de_dados.h"
#include "controle_de_navegacao.h"
#include "logica_de_comando.h"
#include "monitoramento_de_falhas.h"
#include "planejamento_de_rota.h"
#include "tratamento_sensores.h"
#include "buffer_circular_compartilhado.h"
#include "evento_de_falhas.h"

// --- Configuration ---

// --- Global Atomic Flag for Shutdown ---
std::atomic<bool> g_running(true);

/**
 * @brief Handles Ctrl+C (SIGINT) and SIGTERM to shut down gracefully.
 */
void signal_handler(int signum)
{
    std::cout << "\nCaught signal " << signum << ". Shutting down..." << std::endl;
    g_running = false;
}

int main()
{
    std::cout << "[Main] Starting application: " << project_name << " v" << project_version << std::endl;

    SharedCircularBuffer buffer(BUFF_CAPACIDADE, BUFF_CONSUMIDORES);
    FaultEventBus event_bus;
    RouteSharedState route_state;
    // Register signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // --- Connect to the MQTT Broker ---
    std::cout << "Initializing MQTT client..." << std::endl;

    // --- Start the Boost Threads ---
    std::cout << "Starting all threads..." << std::endl;

    // We pass std::ref(client) because the client is not copyable
    boost::thread thread_c(coletor_thread, ID_COLETOR, SLEEP_MS_COLETOR, std::ref(g_running), std::ref(buffer), std::ref(event_bus));
    // boost::thread thread_d(controle_thread, ID_CONTROLE, SLEEP_MS_CONTROLE, std::ref(g_running), std::ref(buffer));
    // boost::thread thread_e(comando_thread, ID_COMANDO, SLEEP_MS_COMANDO, std::ref(g_running), std::ref(buffer));
    boost::thread thread_f(monitoramento_thread, ID_MONITORAMENTO, SLEEP_MS_MONITORAMENTO, std::ref(g_running), std::ref(event_bus));
    boost::thread thread_g(planejamento_thread, ID_PLANEJAMENTO, SLEEP_MS_PLANEJAMENTO, std::ref(g_running), std::ref(buffer), std::ref(route_state));
    boost::thread thread_h(tratamento_thread, ID_TRATAMENTO, SLEEP_MS_TRATAMENTO, std::ref(g_running), std::ref(buffer));
    // --- Wait for Shutdown Signal ---

    // You can also loop here to check g_running
    std::cout << "Publishing threads are running. Press Ctrl+C to stop." << std::endl;
    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Shutdown signal received. Stopping threads..." << std::endl;
    // --- Clean Shutdown ---

    // Interrupt and join threads
    std::cout << "Interrupting threads..." << std::endl;
    thread_c.interrupt();
    // thread_d.interrupt();
    // thread_e.interrupt();
    thread_f.interrupt();
    thread_g.interrupt();
    thread_h.interrupt();

    thread_c.join();
    // thread_d.join();
    // thread_e.join();
    thread_f.join();
    thread_g.join();
    thread_h.join();

    std::cout << "All threads stopped." << std::endl;

    return 0;
}

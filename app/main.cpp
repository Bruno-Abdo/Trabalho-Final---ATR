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

#include "coletor_de_dados.h"
#include "controle_de_navegacao.h"
#include "logica_de_comando.h"
#include "monitoramento_de_falhas.h"
#include "planejamento_de_rota.h"
#include "tratamento_sensores.h"

// --- Configuration ---

// The MQTT broker address. "mosquitto" is the service name
// from your docker-compose.yml
const std::string MQTT_SERVER_ADDRESS{"mqtt://mosquitto:1883"};
const std::string CLIENT_ID_BASE{"cpp_publisher_"};

const std::string TOPIC_A{"test/topic/a"};
const std::string TOPIC_B{"test/topic/b"};
const int QOS = 1;

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

/**
 * @brief The function executed by each Boost thread.
 *
 * It generates JSON messages and publishes them to a specific topic
 * at a regular interval.
 *
 * @param client    Reference to the connected MQTT client.
 * @param topic     The MQTT topic to publish to.
 * @param thread_id A name for this thread (for logging/JSON).
 * @param sleep_ms  Milliseconds to sleep between messages.
 */
void publisher_thread(
    mqtt::async_client &client,
    const std::string &topic,
    const std::string &thread_id,
    int sleep_ms)
{
    int counter = 0;

    // Use nlohmann's JSON library
    using json = nlohmann::json;

    try
    {
        while (g_running)
        {
            // 1. Generate the JSON message
            json msg_json;
            msg_json["source"] = thread_id;
            msg_json["counter"] = counter++;
            msg_json["timestamp"] = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            std::string payload = msg_json.dump();

            // 2. Create a Paho MQTT message
            auto msg = mqtt::make_message(topic, payload);
            msg->set_qos(QOS);

            // 3. Publish the message
            client.publish(msg);

            std::cout << "Published from " << thread_id << ": " << payload << std::endl;

            // 4. Sleep using Boost's interruption-aware sleep
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const mqtt::exception &exc)
    {
        std::cerr << "Error in " << thread_id << ": " << exc.what() << std::endl;
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << thread_id << " was interrupted." << std::endl;
    }
    std::cout << thread_id << " is stopping." << std::endl;
}

int main()
{
    // Register signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // --- Connect to the MQTT Broker ---
    std::cout << "Initializing MQTT client..." << std::endl;
    mqtt::async_client client(MQTT_SERVER_ADDRESS, CLIENT_ID_BASE + "main");

    auto connOpts = mqtt::connect_options_builder()
                        .automatic_reconnect(std::chrono::seconds(2), std::chrono::seconds(30))
                        .clean_session(true)
                        .finalize();

    try
    {
        std::cout << "Connecting to MQTT broker at " << MQTT_SERVER_ADDRESS << "..." << std::endl;
        // Wait for the connection to complete
        client.connect(connOpts)->wait();
        std::cout << "Connection successful!" << std::endl;
    }
    catch (const mqtt::exception &exc)
    {
        std::cerr << "Failed to connect to broker: " << exc.what() << std::endl;
        return 1;
    }

    // --- Start the Boost Threads ---
    std::cout << "Starting all threads..." << std::endl;

    // We pass std::ref(client) because the client is not copyable
    boost::thread thread_a(publisher_thread, std::ref(client), TOPIC_A, "Thread-A", 2000);           // 2 sec interval
    boost::thread thread_b(publisher_thread, std::ref(client), TOPIC_B, "Thread-B", 3000);           // 3 sec interval
    boost::thread thread_c(coletor_thread, "Coletor-Thread", 5000, std::ref(g_running));             // 5 sec interval
    boost::thread thread_d(controle_thread, "Controle-Thread", 7000, std::ref(g_running));           // 7 sec interval
    boost::thread thread_e(comando_thread, "Logica-Thread", 6000, std::ref(g_running));               // 6 sec interval
    boost::thread thread_f(monitoramento_thread, "Monitoramento-Thread", 8000, std::ref(g_running)); // 8 sec interval
    boost::thread thread_g(planejamento_thread, "Planejamento-Thread", 9000, std::ref(g_running));   // 9 sec interval
    boost::thread thread_h(tratamento_thread, "Tratamento-Thread", 1000, std::ref(g_running));       // 1 sec interval

    // --- Wait for Shutdown Signal ---

    // You can also loop here to check g_running
    std::cout << "Publishing threads are running. Press Ctrl+C to stop." << std::endl;
    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Shutdown signal received. Stopping threads..." << std::endl;
    // --- Clean Shutdown ---

    // Interrupt and join threads
    std::cout << "Interrupting threads..." << std::endl;
    thread_a.interrupt();
    thread_b.interrupt();
    thread_c.interrupt();
    thread_d.interrupt();
    thread_e.interrupt();
    thread_f.interrupt();
    thread_g.interrupt();
    thread_h.interrupt();

    thread_a.join();
    thread_b.join();
    thread_c.join();
    thread_d.join();
    thread_e.join();
    thread_f.join();
    thread_g.join();
    thread_h.join();

    std::cout << "All threads stopped." << std::endl;

    // Disconnect from broker
    if (client.is_connected())
    {
        std::cout << "Disconnecting from MQTT broker..." << std::endl;
        client.disconnect()->wait();
        std::cout << "Disconnected." << std::endl;
    }

    return 0;
}

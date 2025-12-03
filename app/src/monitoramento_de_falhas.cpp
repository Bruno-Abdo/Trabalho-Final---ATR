/**
 * @file monitoramento_de_falhas.cpp
 * @brief Tarefa de Monitoramento de Falhas com detecção via MQTT
 * @version 1.0
 * @date 2025-12-03
 */

#include <iostream>
#include <string>
#include <chrono>

#include <boost/thread.hpp>
#include <mqtt/client.h>
#include <nlohmann/json.hpp>

#include "monitoramento_de_falhas.h"
#include "config.hpp"

namespace
{
    using json = nlohmann::json;

    // Constantes de limiar de temperatura (conforme enunciado)
    constexpr int TEMP_ALERT_THRESHOLD = 95;  // °C - Alerta
    constexpr int TEMP_FAULT_THRESHOLD = 120; // °C - Defeito crítico

    /**
     * @brief Estrutura interna para armazenar dados dos sensores de falha
     */
    struct FaultSensorData
    {
        std::string truck_id;
        double timestamp;
        int temperatura;
        bool falha_eletrica;
        bool falha_hidraulica;

        FaultSensorData()
            : truck_id(""),
              timestamp(0.0),
              temperatura(0),
              falha_eletrica(false),
              falha_hidraulica(false)
        {
        }
    };

    /**
     * @brief Parseia payload JSON do MQTT para extrair sensores de falha
     *
     * @param payload String JSON recebida do MQTT
     * @param out Estrutura FaultSensorData a ser preenchida
     * @return true se parsing bem-sucedido, false caso contrário
     */
    bool parse_fault_sensors(const std::string &payload, FaultSensorData &out)
    {
        try
        {
            auto j = json::parse(payload);

            // Extrai truck_id e timestamp
            if (j.contains("truck_id") && j["truck_id"].is_string())
            {
                out.truck_id = j["truck_id"].get<std::string>();
            }

            if (j.contains("timestamp") &&
                (j["timestamp"].is_number_float() || j["timestamp"].is_number_integer()))
            {
                out.timestamp = j["timestamp"].get<double>();
            }

            // Objeto "sensors"
            if (!j.contains("sensors") || !j["sensors"].is_object())
            {
                std::cerr << "[Monitoramento] JSON sem objeto 'sensors'. Ignorando mensagem.\n";
                return false;
            }

            const auto &js = j["sensors"];

            // Extrai temperatura (valor bruto, sem filtro)
            if (js.contains("i_temperatura") && js["i_temperatura"].is_number_integer())
            {
                out.temperatura = js["i_temperatura"].get<int>();
            }

            // Extrai flags de falha
            if (js.contains("i_falha_eletrica") && js["i_falha_eletrica"].is_boolean())
            {
                out.falha_eletrica = js["i_falha_eletrica"].get<bool>();
            }

            if (js.contains("i_falha_hidraulica") && js["i_falha_hidraulica"].is_boolean())
            {
                out.falha_hidraulica = js["i_falha_hidraulica"].get<bool>();
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Monitoramento] Erro ao parsear JSON: " << e.what() << "\n";
            return false;
        }
    }

    /**
     * @brief Avalia sensores e dispara eventos de falha apropriados
     *
     * @param data Dados dos sensores extraídos do MQTT
     * @param event_bus Barramento de eventos para broadcast
     */
    void evaluate_and_dispatch_faults(const FaultSensorData &data, FaultEventBus &event_bus)
    {
        // ========================================
        // 1. AVALIAÇÃO DE TEMPERATURA
        // ========================================
        if (data.temperatura > TEMP_FAULT_THRESHOLD)
        {
            // Condição crítica: T > 120°C
            FaultEvent event;
            event.type = FaultEventType::OvertemperatureFault;
            event.timestamp = data.timestamp;
            event.truck_id = data.truck_id;
            event.temperatura = data.temperatura;
            event.falha_eletrica = data.falha_eletrica;
            event.falha_hidraulica = data.falha_hidraulica;
            event.description = "CRITICO: Temperatura acima de " +
                                std::to_string(TEMP_FAULT_THRESHOLD) +
                                "C (atual: " + std::to_string(data.temperatura) + "C)";

            event_bus.push(event);

            std::cout << "[Monitoramento] FALHA CRÍTICA: " << event.description << "\n";
        }
        else if (data.temperatura > TEMP_ALERT_THRESHOLD)
        {
            // Condição de alerta: T > 95°C (mas <= 120°C)
            FaultEvent event;
            event.type = FaultEventType::OvertemperatureAlert;
            event.timestamp = data.timestamp;
            event.truck_id = data.truck_id;
            event.temperatura = data.temperatura;
            event.falha_eletrica = data.falha_eletrica;
            event.falha_hidraulica = data.falha_hidraulica;
            event.description = "ALERTA: Temperatura acima de " +
                                std::to_string(TEMP_ALERT_THRESHOLD) +
                                "C (atual: " + std::to_string(data.temperatura) + "C)";

            event_bus.push(event);

            std::cout << "[Monitoramento] ALERTA: " << event.description << "\n";
        }

        // ========================================
        // 2. AVALIAÇÃO DE FALHA ELÉTRICA
        // ========================================
        if (data.falha_eletrica)
        {
            FaultEvent event;
            event.type = FaultEventType::ElectricalFault;
            event.timestamp = data.timestamp;
            event.truck_id = data.truck_id;
            event.temperatura = data.temperatura;
            event.falha_eletrica = true;
            event.falha_hidraulica = data.falha_hidraulica;
            event.description = "Falha eletrica detectada";

            event_bus.push(event);

            std::cout << "[Monitoramento] FALHA ELÉTRICA detectada\n";
        }

        // ========================================
        // 3. AVALIAÇÃO DE FALHA HIDRÁULICA
        // ========================================
        if (data.falha_hidraulica)
        {
            FaultEvent event;
            event.type = FaultEventType::HydraulicFault;
            event.timestamp = data.timestamp;
            event.truck_id = data.truck_id;
            event.temperatura = data.temperatura;
            event.falha_eletrica = data.falha_eletrica;
            event.falha_hidraulica = true;
            event.description = "Falha hidraulica detectada";

            event_bus.push(event);

            std::cout << "[Monitoramento] FALHA HIDRÁULICA detectada\n";
        }
    }

} // namespace anônimo

// ========================================================================
// FUNÇÃO PRINCIPAL DA THREAD
// ========================================================================

void monitoramento_thread(int id,
                          int sleep_ms,
                          std::atomic<bool> &running_flag,
                          FaultEventBus &event_bus)
{
    std::cout << "[Monitoramento] Thread " << id << " is starting." << std::endl;

    try
    {
        std::cout << "[Monitoramento] Connecting to MQTT broker at " << MQTT_URL << std::endl;

        // 1) Cria cliente MQTT síncrono dedicado
        mqtt::client client(MQTT_URL, "monitoramento_client");

        mqtt::connect_options conn_opts;
        conn_opts.set_clean_session(true);
        client.connect(conn_opts);

        std::cout << "[Monitoramento] Connected to broker. Subscribing to topic '"
                  << MQTT_TOPIC_SENSORS << "'\n";

        // 2) Subscribe no tópico dos sensores (mesmo tópico que Tratamento)
        client.subscribe(MQTT_TOPIC_SENSORS, MQTT_DEFAULT_QOS);

        // 3) Loop principal: polling + detecção de falhas
        while (running_flag.load())
        {
            // Permite interrupção via boost::thread::interrupt()
            boost::this_thread::interruption_point();

            mqtt::const_message_ptr msg;

            // Espera até 'sleep_ms' por uma mensagem nova
            bool got_msg = client.try_consume_message_for(
                &msg,
                std::chrono::milliseconds(sleep_ms));

            if (!got_msg)
            {
                // Sem mensagem nova nesse período -> volta pro topo do loop
                continue;
            }

            if (!msg)
            {
                // Mensagem nula (caso raro) -> ignora
                continue;
            }

            const std::string payload = msg->get_payload_str();
            FaultSensorData sensor_data;

            if (!parse_fault_sensors(payload, sensor_data))
            {
                // JSON inválido -> apenas loga o erro e ignora
                continue;
            }

            // 4) Avalia condições de falha e dispara eventos
            evaluate_and_dispatch_faults(sensor_data, event_bus);

            // (Opcional) Log reduzido para debug:
            // std::cout << "[Monitoramento] Processou amostra: "
            //           << "truck_id=" << sensor_data.truck_id
            //           << ", T=" << sensor_data.temperatura
            //           << ", FE=" << sensor_data.falha_eletrica
            //           << ", FH=" << sensor_data.falha_hidraulica
            //           << std::endl;
        }

        // 5) Desconecta ao sair do loop
        std::cout << "[Monitoramento] Disconnecting MQTT client..." << std::endl;
        client.disconnect();
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Monitoramento] Thread " << id << " was interrupted." << std::endl;
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "[Monitoramento] MQTT exception: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Monitoramento] std::exception: " << e.what() << std::endl;
    }

    std::cout << "[Monitoramento] Thread " << id << " is stopping." << std::endl;
}

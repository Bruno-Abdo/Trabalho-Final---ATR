/**
 * @file logica_de_comando.cpp
 * @brief Implementação da tarefa Lógica de Comando (Versão Híbrida)
 * @version 1.0
 * @date 2025-12-03
 */

#include "logica_de_comando.h"

#include <iostream>
#include <chrono>
#include <cmath>

#include <boost/thread.hpp>
#include <mqtt/client.h>
#include <nlohmann/json.hpp>

#include "config.hpp"

using json = nlohmann::json;

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

namespace
{
    template <typename T>
    T clamp(T value, T min_v, T max_v)
    {
        if (value < min_v)
            return min_v;
        if (value > max_v)
            return max_v;
        return value;
    }

    /**
     * @brief Estrutura para armazenar comando manual da Interface Local.
     */
    struct ManualCommand
    {
        int accel{0}; ///< Aceleração manual [-100, +100]
        int steer{0}; ///< Direção manual [-30, +30]
        bool valid{false};
        std::string truck_id{"001"};
    };

    /**
     * @brief Parseia comando da Interface Local a partir de JSON.
     *
     * Formato esperado:
     * {
     *   "truck_id": "001",
     *   "mode": "manual" | "automatico",  // Opcional: alterna modo
     *   "o_aceleracao": 50,               // Comando manual de aceleração
     *   "o_direcao": -10                  // Comando manual de direção
     * }
     *
     * @param payload        String JSON recebida via MQTT.
     * @param out_cmd        Comando manual preenchido (acel/dir/truck_id).
     * @param request_auto   Seta true se a mensagem pedir modo automático.
     * @param request_manual Seta true se a mensagem pedir modo manual.
     * @return true se parse bem-sucedido, false caso contrário.
     */
    bool parse_interface_command(const std::string &payload,
                                 ManualCommand &out_cmd,
                                 bool &request_auto,
                                 bool &request_manual)
    {
        request_auto = false;
        request_manual = false;

        try
        {
            auto j = json::parse(payload);

            if (j.contains("truck_id") && j["truck_id"].is_string())
                out_cmd.truck_id = j["truck_id"].get<std::string>();

            if (j.contains("mode") && j["mode"].is_string())
            {
                const std::string mode = j["mode"].get<std::string>();
                if (mode == "automatico" || mode == "auto")
                    request_auto = true;
                else if (mode == "manual")
                    request_manual = true;
            }

            if (j.contains("o_aceleracao") && j["o_aceleracao"].is_number_integer())
                out_cmd.accel = j["o_aceleracao"].get<int>();

            if (j.contains("o_direcao") && j["o_direcao"].is_number_integer())
                out_cmd.steer = j["o_direcao"].get<int>();

            // Saturação básica de segurança
            out_cmd.accel = clamp(out_cmd.accel, -100, 100);
            out_cmd.steer = clamp(out_cmd.steer, -30, 30);

            out_cmd.valid = true;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Comando] Erro ao parsear JSON da interface: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Atualiza LogicSharedState com base em evento de falha.
     *
     * Regra simples:
     *  - OvertemperatureFault / ElectricalFault / HydraulicFault -> e_defeito = true
     *  - FaultCleared                                           -> e_defeito = false
     */
    void apply_fault_event_to_logic(const FaultEvent &ev, LogicSharedState &logic_state)
    {
        bool set_defect_true = false;
        bool set_defect_false = false;

        switch (ev.type)
        {
        case FaultEventType::OvertemperatureFault:
        case FaultEventType::ElectricalFault:
        case FaultEventType::HydraulicFault:
            set_defect_true = true;
            break;

        case FaultEventType::FaultCleared:
            set_defect_false = true;
            break;

        case FaultEventType::OvertemperatureAlert:
            // Só alerta, não força defeito
            break;
        }

        if (set_defect_true || set_defect_false)
        {
            boost::lock_guard<boost::mutex> lock(logic_state.mtx);
            if (set_defect_true)
                logic_state.e_defeito = true;
            if (set_defect_false)
                logic_state.e_defeito = false;
        }
    }
} // namespace

// ============================================================================
// THREAD PRINCIPAL: LÓGICA DE COMANDO
// ============================================================================

void comando_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer,
                    FaultEventBus &event_bus,
                    NavigationControlState &nav_state,
                    LogicSharedState &logic_state)
{
    std::cout << "[Comando " << id << "] Thread de Lógica de Comando iniciando..." << std::endl;

    // ========================================================================
    // INICIALIZAÇÃO: Estado padrão
    // ========================================================================
    {
        boost::lock_guard<boost::mutex> lock(logic_state.mtx);
        logic_state.e_automatico = true; // Começa em modo automático
        logic_state.e_defeito = false;
    }
    std::cout << "[Comando " << id << "] Estado inicial: MODO AUTOMÁTICO ativado." << std::endl;

    // Cliente MQTT síncrono para Interface Local + Atuadores
    std::string client_id = std::string(MQTT_CLIENT_ID) + "_command";
    mqtt::client client(std::string(MQTT_URL), client_id);

    ManualCommand manual_cmd; // Último comando manual recebido
    std::string current_truck_id{"001"};

    try
    {
        // --------------------------------------------------------------------
        // CONEXÃO MQTT
        // --------------------------------------------------------------------
        mqtt::connect_options conn_opts;
        conn_opts.set_clean_session(true);

        std::cout << "[Comando " << id << "] Conectando ao broker MQTT em "
                  << MQTT_URL << " com client_id=" << client_id << std::endl;

        client.connect(conn_opts);
        client.start_consuming();
        client.subscribe(MQTT_TOPIC_INTERFACE, MQTT_DEFAULT_QOS);

        std::cout << "[Comando " << id << "] Inscrito em " << MQTT_TOPIC_INTERFACE
                  << " (Interface Local)" << std::endl;

        // --------------------------------------------------------------------
        // LOOP PRINCIPAL
        // --------------------------------------------------------------------
        int log_counter = 0;
        std::string last_mode = "auto"; // Para detectar mudanças de modo

        while (running_flag.load())
        {
            boost::this_thread::interruption_point();

            // ----------------------------------------------------------------
            // 1. PROCESSAR EVENTOS DE FALHA (não bloqueante)
            // ----------------------------------------------------------------
            {
                FaultEvent ev;
                while (event_bus.try_pop(FaultEventBus::Consumer::Logica, ev))
                {
                    apply_fault_event_to_logic(ev, logic_state);

                    // Log de eventos de falha
                    if (ev.type == FaultEventType::OvertemperatureFault ||
                        ev.type == FaultEventType::ElectricalFault ||
                        ev.type == FaultEventType::HydraulicFault)
                    {
                        std::cout << "[Comando " << id << "] DEFEITO CRÍTICO: "
                                  << ev.description << " | e_defeito = TRUE" << std::endl;
                    }
                    else if (ev.type == FaultEventType::FaultCleared)
                    {
                        std::cout << "[Comando " << id << "] Falhas limpas. "
                                  << "e_defeito = FALSE" << std::endl;
                    }
                }
            }

            // ----------------------------------------------------------------
            // 2. LER COMANDOS DA INTERFACE LOCAL (MQTT) - polling curto
            // ----------------------------------------------------------------
            {
                mqtt::const_message_ptr msg;
                bool got_msg = client.try_consume_message_for(
                    &msg,
                    std::chrono::milliseconds(5));

                if (got_msg && msg)
                {
                    bool request_auto = false;
                    bool request_manual = false;
                    ManualCommand tmp_cmd;

                    const std::string payload = msg->get_payload_str();
                    if (parse_interface_command(payload, tmp_cmd, request_auto, request_manual))
                    {
                        manual_cmd = tmp_cmd;
                        current_truck_id = manual_cmd.truck_id;

                        // Atualiza modo de operação
                        {
                            boost::lock_guard<boost::mutex> lock(logic_state.mtx);
                            if (request_auto && !logic_state.e_automatico)
                            {
                                logic_state.e_automatico = true;
                                std::cout << "[Comando " << id << "] Modo AUTOMÁTICO ativado "
                                          << "(via Interface Local)" << std::endl;
                            }
                            if (request_manual && logic_state.e_automatico)
                            {
                                logic_state.e_automatico = false;
                                std::cout << "[Comando " << id << "] Modo MANUAL ativado "
                                          << "(via Interface Local)" << std::endl;
                            }
                        }
                    }
                }
            }

            // ----------------------------------------------------------------
            // 3. SNAPSHOT DE MODO/DEFEITO (leitura thread-safe)
            // ----------------------------------------------------------------
            bool automatico = false;
            bool defeito = false;
            {
                boost::lock_guard<boost::mutex> lock(logic_state.mtx);
                automatico = logic_state.e_automatico;
                defeito = logic_state.e_defeito;
            }

            // ----------------------------------------------------------------
            // 4. ARBITRAGEM: Combinar comandos (manual x automático x defeito)
            // ----------------------------------------------------------------
            int o_aceleracao = 0;
            int o_direcao = 0;
            std::string source = "idle";

            if (defeito)
            {
                // PROTOCOLO DE SEGURANÇA: Em defeito, zera atuadores (fail-safe)
                o_aceleracao = 0;
                o_direcao = 0;
                source = "fault";
            }
            else if (automatico)
            {
                // Tenta usar Controle de Navegação
                bool use_auto = false;
                int auto_accel = 0;
                int auto_steer = 0;

                {
                    boost::lock_guard<boost::mutex> lock(nav_state.mtx);
                    if (nav_state.u_valido)
                    {
                        auto_accel = nav_state.u_aceleracao_auto;
                        auto_steer = nav_state.u_direcao_auto;
                        use_auto = true;
                    }
                }

                if (use_auto)
                {
                    o_aceleracao = auto_accel;
                    o_direcao = auto_steer;
                    source = "auto";
                }
                else if (manual_cmd.valid)
                {
                    // Fallback inteligente: se o controle automático ainda não for válido,
                    // mas existir comando manual, utiliza o manual.
                    o_aceleracao = manual_cmd.accel;
                    o_direcao = manual_cmd.steer;
                    source = "manual_fallback";
                }
                else
                {
                    // Sem controle automático válido e sem comando manual: neutro
                    o_aceleracao = 0;
                    o_direcao = 0;
                    source = "idle";
                }
            }
            else
            {
                // MODO MANUAL
                if (manual_cmd.valid)
                {
                    o_aceleracao = manual_cmd.accel;
                    o_direcao = manual_cmd.steer;
                    source = "manual";
                }
                else
                {
                    // Sem comando manual ainda → neutro
                    o_aceleracao = 0;
                    o_direcao = 0;
                    source = "idle";
                }
            }

            // Segurança extra: saturar (mesmo que já saturado em etapas anteriores)
            o_aceleracao = clamp(o_aceleracao, -100, 100);
            o_direcao = clamp(o_direcao, -30, 30);

            // ----------------------------------------------------------------
            // 5. PUBLICAR ATUADORES NO MQTT
            // ----------------------------------------------------------------
            try
            {
                const double now_sec = std::chrono::duration<double>(
                                           std::chrono::system_clock::now().time_since_epoch())
                                           .count();

                json j;
                j["truck_id"] = current_truck_id;
                j["timestamp"] = now_sec;

                json act;
                act["o_aceleracao"] = o_aceleracao;
                act["o_direcao"] = o_direcao;
                act["source"] = source; // Telemetria: origem dos comandos

                j["actuators"] = act;

                auto payload = j.dump();

                auto msg = mqtt::make_message(MQTT_TOPIC_ACTUATORS, payload);
                msg->set_qos(MQTT_DEFAULT_QOS);
                client.publish(msg);

                // Log periódico (a cada ~50 ciclos)
                if (log_counter % 50 == 0)
                {
                    std::cout << "[Comando " << id << "] "
                              << "source=" << source
                              << " | o_acel=" << o_aceleracao
                              << " | o_dir=" << o_direcao << "°" << std::endl;
                }
            }
            catch (const mqtt::exception &e)
            {
                std::cerr << "[Comando " << id
                          << "] Erro ao publicar atuadores: " << e.what() << std::endl;
            }

            ++log_counter;

            // ----------------------------------------------------------------
            // 6. AGUARDAR PRÓXIMO CICLO (frequência de atuação)
            // ----------------------------------------------------------------
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }

        // --------------------------------------------------------------------
        // ENCERRAMENTO MQTT
        // --------------------------------------------------------------------
        if (client.is_connected())
        {
            try
            {
                client.unsubscribe(MQTT_TOPIC_INTERFACE);
                client.stop_consuming();
                client.disconnect();
                std::cout << "[Comando " << id << "] MQTT desconectado." << std::endl;
            }
            catch (const mqtt::exception &e)
            {
                std::cerr << "[Comando " << id
                          << "] Erro ao desconectar MQTT: " << e.what() << std::endl;
            }
        }
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Comando " << id << "] Thread interrompida." << std::endl;

        if (client.is_connected())
        {
            try
            {
                client.unsubscribe(MQTT_TOPIC_INTERFACE);
                client.stop_consuming();
                client.disconnect();
            }
            catch (const mqtt::exception &e)
            {
                std::cerr << "[Comando " << id
                          << "] Erro ao desconectar MQTT (interrupted): " << e.what() << std::endl;
            }
        }
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "[Comando " << id << "] MQTT exception: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Comando " << id << "] std::exception: " << e.what() << std::endl;
    }

    std::cout << "[Comando " << id << "] Thread de Lógica de Comando encerrada." << std::endl;
}

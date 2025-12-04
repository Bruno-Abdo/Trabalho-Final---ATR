/**
 * @file planejamento_de_rota.cpp
 * @brief Implementação da tarefa de Planejamento de Rota com interface MQTT
 */

#include "planejamento_de_rota.h"

#include <iostream>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <string>
#include <chrono>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#include <nlohmann/json.hpp>
#include <mqtt/client.h>

// Alias para JSON
using json = nlohmann::json;

// ----------------------------------------------------------------------------
// Configuração interna
// ----------------------------------------------------------------------------

namespace
{
    // Tolerância padrão se não for especificada via MQTT
    constexpr double DEFAULT_WAYPOINT_TOLERANCE = 2.0;

    struct RouteCommand
    {
        int start_x{0};
        int start_y{0};
        int goal_x{0};
        int goal_y{0};
        std::size_t num_waypoints{2};
        double tolerance{DEFAULT_WAYPOINT_TOLERANCE};
    };

    /**
     * @brief Faz o parse de um comando de rota em JSON.
     *
     * Formato esperado:
     * {
     *   "truck_id": "001",
     *   "start_x": int,
     *   "start_y": int,
     *   "goal_x": int,
     *   "goal_y": int,
     *   "num_waypoints": int (opcional, >= 2),
     *   "tolerance": double (opcional)
     * }
     */
    std::optional<RouteCommand> parse_route_command(const std::string &payload)
    {
        try
        {
            json j = json::parse(payload);

            RouteCommand cmd;
            cmd.start_x = j.at("start_x").get<int>();
            cmd.start_y = j.at("start_y").get<int>();
            cmd.goal_x = j.at("goal_x").get<int>();
            cmd.goal_y = j.at("goal_y").get<int>();

            // num_waypoints: se não vier, usa distância como aproximação
            if (j.contains("num_waypoints") && j["num_waypoints"].is_number_integer())
            {
                int n = j["num_waypoints"].get<int>();
                if (n < 2)
                {
                    std::cerr << "[Planejamento] num_waypoints < 2 no comando MQTT, ajustando para 2." << std::endl;
                    n = 2;
                }
                cmd.num_waypoints = static_cast<std::size_t>(n);
            }
            else
            {
                // Aproxima: 1 waypoint por unidade de distância
                const double dx = static_cast<double>(cmd.goal_x - cmd.start_x);
                const double dy = static_cast<double>(cmd.goal_y - cmd.start_y);
                const double dist = std::sqrt(dx * dx + dy * dy);

                std::size_t n = static_cast<std::size_t>(std::ceil(dist));
                if (n < 2)
                    n = 2;
                cmd.num_waypoints = n;
            }

            // tolerance: opcional
            if (j.contains("tolerance") && j["tolerance"].is_number())
            {
                cmd.tolerance = j["tolerance"].get<double>();
            }
            else
            {
                cmd.tolerance = DEFAULT_WAYPOINT_TOLERANCE;
            }

            std::cout << "[Planejamento] Comando de rota recebido via MQTT: "
                      << "start=(" << cmd.start_x << "," << cmd.start_y << "), "
                      << "goal=(" << cmd.goal_x << "," << cmd.goal_y << "), "
                      << "num_waypoints=" << cmd.num_waypoints << ", "
                      << "tolerance=" << cmd.tolerance << std::endl;

            return cmd;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Planejamento] Erro ao parsear comando de rota MQTT: "
                      << e.what() << " | payload=" << payload << std::endl;
            return std::nullopt;
        }
    }

} // namespace

// ============================================================================
// FUNÇÕES AUXILIARES DE MANIPULAÇÃO DE ROTA
// ============================================================================

void clear_route(RouteSharedState &state)
{
    // IMPORTANTE: mtx é boost::mutex na sua base de código
    boost::lock_guard<boost::mutex> lock(state.mtx);

    state.waypoints.clear();
    state.current_index = 0;
    state.route_active = false;
    state.route_completed = false;

    std::cout << "[Planejamento] Rota limpa (cancelada)." << std::endl;
}

void set_new_route(RouteSharedState &state,
                   const std::vector<Waypoint> &waypoints)
{
    boost::lock_guard<boost::mutex> lock(state.mtx);

    state.waypoints = waypoints;
    state.current_index = 0;
    state.route_active = !waypoints.empty();
    state.route_completed = false;

    if (!waypoints.empty())
    {
        std::cout << "[Planejamento] Nova rota definida com "
                  << waypoints.size() << " waypoints." << std::endl;
        std::cout << "[Planejamento] Start: (" << waypoints.front().x
                  << ", " << waypoints.front().y << ") -> Goal: ("
                  << waypoints.back().x << ", " << waypoints.back().y
                  << ")" << std::endl;
    }
    else
    {
        std::cout << "[Planejamento] Rota vazia recebida (ignorada)." << std::endl;
    }
}

std::vector<Waypoint> generate_linear_route(int start_x, int start_y,
                                            int goal_x, int goal_y,
                                            std::size_t num_waypoints)
{
    if (num_waypoints < 2)
    {
        throw std::invalid_argument(
            "[Planejamento] generate_linear_route: num_waypoints deve ser >= 2. "
            "Recebido: " +
            std::to_string(num_waypoints));
    }

    std::vector<Waypoint> waypoints;
    waypoints.reserve(num_waypoints);

    const double dx = static_cast<double>(goal_x - start_x);
    const double dy = static_cast<double>(goal_y - start_y);
    const double steps = static_cast<double>(num_waypoints - 1);

    for (std::size_t i = 0; i < num_waypoints; ++i)
    {
        const double t = static_cast<double>(i) / steps;

        const double xf = static_cast<double>(start_x) + t * dx;
        const double yf = static_cast<double>(start_y) + t * dy;

        const int x = static_cast<int>(std::round(xf));
        const int y = static_cast<int>(std::round(yf));

        waypoints.emplace_back(x, y);
    }

    std::cout << "[Planejamento] Rota linear gerada: ("
              << start_x << ", " << start_y << ") -> ("
              << goal_x << ", " << goal_y << ") com "
              << num_waypoints << " waypoints." << std::endl;

    return waypoints;
}

bool get_current_waypoint(const RouteSharedState &state,
                          Waypoint &out_waypoint)
{
    boost::lock_guard<boost::mutex> lock(state.mtx);

    if (!state.route_active || state.route_completed)
    {
        return false;
    }

    if (state.current_index >= state.waypoints.size())
    {
        return false;
    }

    out_waypoint = state.waypoints[state.current_index];
    return true;
}

// ============================================================================
// THREAD PRINCIPAL DE PLANEJAMENTO DE ROTA
// ============================================================================

void planejamento_thread(int id,
                         int sleep_ms,
                         std::atomic<bool> &running_flag,
                         SharedCircularBuffer &buffer,
                         RouteSharedState &route_state)
{
    std::cout << "[Planejamento " << id << "] Thread iniciando..." << std::endl;

    // Cliente MQTT síncrono (igual ao Tratamento, mas com client_id próprio)
    std::string client_id = std::string(MQTT_CLIENT_ID) + "_route";
    mqtt::client client(std::string(MQTT_URL), client_id);

    // Tolerância de waypoint (pode ser atualizada via MQTT)
    double waypoint_tolerance = DEFAULT_WAYPOINT_TOLERANCE;

    try
    {
        // --------------------------------------------------------------------
        // Conexão MQTT e assinatura do tópico de rota
        // --------------------------------------------------------------------
        mqtt::connect_options conn_opts;
        conn_opts.set_clean_session(true);

        std::cout << "[Planejamento " << id << "] Conectando ao broker MQTT em "
                  << MQTT_URL << " com client_id=" << client_id << std::endl;

        client.connect(conn_opts);
        client.start_consuming();
        client.subscribe(MQTT_TOPIC_ROUTE, MQTT_DEFAULT_QOS);

        std::cout << "[Planejamento " << id << "] Assinado em "
                  << MQTT_TOPIC_ROUTE << std::endl;

        // --------------------------------------------------------------------
        // Rota inicial fixa (fallback se nenhum comando MQTT chegar)
        // --------------------------------------------------------------------
        const int START_X = 0;
        const int START_Y = 0;
        const int GOAL_X = 100;
        const int GOAL_Y = 50;

        const double dx0 = static_cast<double>(GOAL_X - START_X);
        const double dy0 = static_cast<double>(GOAL_Y - START_Y);
        double dist_total = std::sqrt(dx0 * dx0 + dy0 * dy0);
        std::size_t num_waypoints = static_cast<std::size_t>(std::ceil(dist_total));
        if (num_waypoints < 2)
            num_waypoints = 2;

        auto initial_route = generate_linear_route(START_X, START_Y, GOAL_X, GOAL_Y, num_waypoints);
        set_new_route(route_state, initial_route);

        // --------------------------------------------------------------------
        // Loop principal: lê comandos de rota via MQTT e atualiza progresso
        // --------------------------------------------------------------------
        while (running_flag.load())
        {
            boost::this_thread::interruption_point();

            mqtt::const_message_ptr msg;
            bool got_msg = client.try_consume_message_for(&msg, std::chrono::milliseconds(10));
            if (got_msg && msg)
            {
                const std::string payload = msg->to_string();
                auto cmd_opt = parse_route_command(payload);
                if (cmd_opt.has_value())
                {
                    const RouteCommand &cmd = *cmd_opt;

                    auto new_route = generate_linear_route(
                        cmd.start_x, cmd.start_y,
                        cmd.goal_x, cmd.goal_y,
                        cmd.num_waypoints);

                    set_new_route(route_state, new_route);
                    waypoint_tolerance = cmd.tolerance;
                }
            }

            // 2) Lê posição atual do caminhão do buffer (consumidor padrão)
            BufferData current_data = buffer.read(id, running_flag);

            if (!running_flag.load())
                break;

            // 3) Obtém waypoint corrente da rota
            Waypoint current_waypoint;
            bool has_waypoint = get_current_waypoint(route_state, current_waypoint);

            if (!has_waypoint)
            {
                // Rota inativa ou concluída: apenas aguarda próximo ciclo
                boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
                continue;
            }

            // 4) Calcula distância até o waypoint
            const double dx = static_cast<double>(current_data.i_posicao_x - current_waypoint.x);
            const double dy = static_cast<double>(current_data.i_posicao_y - current_waypoint.y);
            const double distance = std::sqrt(dx * dx + dy * dy);

            // Debug opcional
            // std::cout << "[Planejamento " << id << "] pos=("
            //           << current_data.i_posicao_x << "," << current_data.i_posicao_y
            //           << ") waypoint=(" << current_waypoint.x << "," << current_waypoint.y
            //           << ") dist=" << distance << std::endl;

            // 5) Se dentro da tolerância, avança para o próximo waypoint
            if (distance < waypoint_tolerance)
            {
                boost::lock_guard<boost::mutex> lock(route_state.mtx);

                route_state.current_index++;

                std::cout << "[Planejamento " << id
                          << "] Waypoint alcançado! Avançando para índice "
                          << route_state.current_index << "/"
                          << route_state.waypoints.size() << std::endl;

                // Verifica se completou a rota
                if (route_state.current_index >= route_state.waypoints.size())
                {
                    route_state.route_completed = true;
                    route_state.route_active = false;

                    std::cout << "[Planejamento " << id
                              << "] *** ROTA CONCLUÍDA! ***" << std::endl;
                }
            }

            // 6) Espera até o próximo ciclo
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }

        // --------------------------------------------------------------------
        // Encerramento MQTT
        // --------------------------------------------------------------------
        if (client.is_connected())
        {
            try
            {
                client.unsubscribe(MQTT_TOPIC_ROUTE);
                client.stop_consuming();
                client.disconnect();
            }
            catch (const mqtt::exception &e)
            {
                std::cerr << "[Planejamento " << id
                          << "] Erro ao desconectar MQTT: " << e.what() << std::endl;
            }
        }

        std::cout << "[Planejamento " << id << "] Thread finalizando (running=false)." << std::endl;
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Planejamento " << id << "] Thread interrompida." << std::endl;

        // Tenta encerrar MQTT de forma graciosa
        if (client.is_connected())
        {
            try
            {
                client.unsubscribe(MQTT_TOPIC_ROUTE);
                client.stop_consuming();
                client.disconnect();
            }
            catch (const mqtt::exception &e)
            {
                std::cerr << "[Planejamento " << id
                          << "] Erro ao desconectar MQTT (interrupted): " << e.what() << std::endl;
            }
        }
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "[Planejamento " << id << "] MQTT exception: "
                  << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Planejamento " << id << "] std::exception: "
                  << e.what() << std::endl;
    }
    std::cout << "[Planejamento " << id << "] Thread encerrada." << std::endl;
}

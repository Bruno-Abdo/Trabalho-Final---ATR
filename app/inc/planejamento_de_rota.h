#ifndef PLANEJAMENTO_DE_ROTA_H
#define PLANEJAMENTO_DE_ROTA_H

#include <vector>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <boost/thread.hpp>

#include "config.hpp"
#include "buffer_circular_compartilhado.h"

/**
 * @brief Ponto de passagem (waypoint) em coordenadas inteiras.
 *
 * Coerente com i_posicao_x e i_posicao_y de BufferData (valores tratados).
 */
struct Waypoint
{
    int x{0};
    int y{0};

    Waypoint() = default;
    Waypoint(int px, int py) : x(px), y(py) {}
};

/**
 * @brief Estado compartilhado da rota entre Planejamento de Rota e Controle de Navegação.
 *
 * Esta estrutura mantém:
 *  - a lista de waypoints da rota atual;
 *  - o índice do waypoint corrente;
 *  - flags de rota ativa e rota concluída;
 *  - um mutex para proteger acesso concorrente.
 */
struct RouteSharedState
{
    std::vector<Waypoint> waypoints;
    std::size_t current_index{0};
    bool route_active{false};
    bool route_completed{false};

    mutable boost::mutex mtx; ///< Protege todo o estado acima.
};

/**
 * @brief Limpa completamente a rota atual, deixando o estado inativo.
 *
 * @param state Estado de rota a ser limpo.
 *
 * @note Thread-safe: trava mtx internamente.
 * @note Útil para cancelamento de rotas em caso de falhas críticas.
 */
void clear_route(RouteSharedState &state);

/**
 * @brief Define uma nova rota a partir de um vetor de waypoints.
 *
 * Esta função:
 *  - substitui a lista de waypoints;
 *  - zera current_index;
 *  - marca route_active = true e route_completed = false.
 *
 * @param state Estado de rota a ser atualizado.
 * @param waypoints Vetor de waypoints que define a rota (cópia interna).
 *
 * @note Thread-safe: trava mtx internamente.
 * @note Para rotas geradas via MQTT, JSON ou algoritmos externos.
 */
void set_new_route(RouteSharedState &state,
                   const std::vector<Waypoint> &waypoints);

/**
 * @brief Gera waypoints interpolados linearmente entre start e goal.
 *
 * @param start_x Coordenada X inicial.
 * @param start_y Coordenada Y inicial.
 * @param goal_x Coordenada X final.
 * @param goal_y Coordenada Y final.
 * @param num_waypoints Número total de pontos (incluindo start e goal, min=2).
 * @return std::vector<Waypoint> Lista de waypoints interpolados.
 *
 * @note Algoritmo: waypoint[i] = start + i * (goal - start) / (num_waypoints - 1)
 * @note Função auxiliar para caso comum; rotas complexas devem usar set_new_route diretamente.
 * @throws std::invalid_argument se num_waypoints < 2.
 *
 * Exemplo de uso:
 *   auto wps = generate_linear_route(0, 0, 100, 50, 50);
 *   set_new_route(route_state, wps);
 */
std::vector<Waypoint> generate_linear_route(int start_x, int start_y,
                                            int goal_x, int goal_y,
                                            std::size_t num_waypoints);

/**
 * @brief Obtém, de forma thread-safe, o waypoint corrente (setpoint de posição).
 *
 * @param state Estado de rota compartilhado.
 * @param out_waypoint Saída: waypoint corrente, se existir.
 * @return true se existe waypoint válido (rota ativa e não concluída), false caso contrário.
 *
 * @note Thread-safe: trava mtx internamente.
 * @note Uso típico no Controle de Navegação (modo automático):
 *       Waypoint target;
 *       if (get_current_waypoint(g_route_state, target)) {
 *           // Calcular erro: target.x - current_x
 *           // Executar controle
 *       }
 */
bool get_current_waypoint(const RouteSharedState &state,
                          Waypoint &out_waypoint);

/**
 * @brief Thread de Planejamento de Rota.
 *
 * Versão mínima:
 *  1. Em sua inicialização, define uma rota (por exemplo, via generate_linear_route)
 *     e aplica com set_new_route.
 *  2. Em cada ciclo:
 *     - lê a posição atual do caminhão via buffer.read(id, running_flag);
 *     - obtém o waypoint corrente via get_current_waypoint;
 *     - calcula a distância até o waypoint;
 *     - avança current_index se a distância for menor que uma tolerância (WAYPOINT_TOLERANCE);
 *     - marca route_completed quando current_index >= waypoints.size().
 *
 * Extensões futuras:
 *  - Receber comandos de rota via MQTT (start/goal dinâmicos);
 *  - Cancelar rota em caso de falhas críticas.
 *
 * @param id Identificador lógico da tarefa (ex: ID_PLANEJAMENTO).
 * @param sleep_ms Período da tarefa em milissegundos (ex: SLEEP_MS_PLANEJAMENTO).
 * @param running_flag Flag global de execução (false => shutdown).
 * @param buffer Buffer circular compartilhado com dados tratados dos sensores.
 * @param route_state Estado compartilhado da rota (waypoints e progresso).
 */
void planejamento_thread(int id,
                         int sleep_ms,
                         std::atomic<bool> &running_flag,
                         SharedCircularBuffer &buffer,
                         RouteSharedState &route_state);

#endif // PLANEJAMENTO_DE_ROTA_H

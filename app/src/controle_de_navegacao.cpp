/**
 * @file controle_de_navegacao.cpp
 * @brief Implementação da tarefa de Controle de Navegação (Piloto Automático)
 * @version 1.0
 * @date 2025-12-03
 */

#include "controle_de_navegacao.h"

#include <iostream>
#include <cmath>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

namespace
{
    constexpr double RAD2DEG = 180.0 / 3.14159265358979323846;

    /**
     * @brief Normaliza ângulo em graus para o intervalo [-180, +180].
     */
    double normalize_angle_deg(double angle)
    {
        angle = std::fmod(angle, 360.0); // [-360, +360]

        if (angle > 180.0)
            angle -= 360.0;
        else if (angle < -180.0)
            angle += 360.0;

        return angle;
    }

    /**
     * @brief Satura valor no intervalo [vmin, vmax].
     */
    template <typename T>
    T clamp(T value, T vmin, T vmax)
    {
        if (value < vmin)
            return vmin;
        if (value > vmax)
            return vmax;
        return value;
    }

    // Distância abaixo da qual consideramos que o waypoint foi atingido.
    constexpr double EPS_DIST = 0.1; // [m]
}

// ============================================================================
// THREAD PRINCIPAL: CONTROLE DE NAVEGAÇÃO
// ============================================================================

void controle_thread(int id,
                     int sleep_ms,
                     std::atomic<bool> &running_flag,
                     SharedCircularBuffer &buffer,
                     RouteSharedState &route_state,
                     LogicSharedState &logic_state,
                     NavigationControlState &nav_state)
{
    std::cout << "[Controle " << id << "] Thread de Controle de Navegacao iniciando..." << std::endl;

    try
    {
        int log_counter = 0;
        bool logged_inactive = false;
        bool logged_no_route = false;

        while (running_flag.load())
        {
            boost::this_thread::interruption_point();

            // ----------------------------------------------------------------
            // 1. LER ESTADO ATUAL DO CAMINHÃO (buffer circular bloqueante)
            // ----------------------------------------------------------------
            BufferData current_data = buffer.read(id, running_flag);
            if (!running_flag.load())
                break;

            const int pos_x = current_data.i_posicao_x;
            const int pos_y = current_data.i_posicao_y;
            const int angulo_atual = current_data.i_angulo_x; // [graus]

            // ----------------------------------------------------------------
            // 2. LER MODO DE OPERAÇÃO (Lógica de Comando)
            // ----------------------------------------------------------------
            bool modo_automatico = false;
            bool defeito_ativo = false;

            {
                boost::lock_guard<boost::mutex> lock(logic_state.mtx);
                modo_automatico = logic_state.e_automatico;
                defeito_ativo = logic_state.e_defeito;
            }

            // ----------------------------------------------------------------
            // 3. VERIFICAR CONDIÇÕES PARA CONTROLE AUTOMÁTICO
            // ----------------------------------------------------------------
            if (!modo_automatico || defeito_ativo)
            {
                // Estado neutro / comandos inválidos
                {
                    boost::lock_guard<boost::mutex> lock(nav_state.mtx);
                    nav_state.u_aceleracao_auto = 0;
                    nav_state.u_direcao_auto = 0;
                    nav_state.u_valido = false;
                }

                if (!logged_inactive)
                {
                    std::cout << "[Controle " << id << "] Modo manual ou defeito ativo. "
                              << "Controle automático desabilitado." << std::endl;
                    logged_inactive = true;
                }

                boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
                continue;
            }
            else
            {
                logged_inactive = false;
            }

            // ----------------------------------------------------------------
            // 4. OBTER WAYPOINT ATUAL (Planejamento de Rota) – API GLOBAL
            // ----------------------------------------------------------------
            Waypoint target_waypoint;
            bool has_waypoint = get_current_waypoint(route_state, target_waypoint);

            if (!has_waypoint)
            {
                {
                    boost::lock_guard<boost::mutex> lock(nav_state.mtx);
                    nav_state.u_aceleracao_auto = 0;
                    nav_state.u_direcao_auto = 0;
                    nav_state.u_valido = false;
                }

                if (!logged_no_route)
                {
                    std::cout << "[Controle " << id << "] Sem rota ativa. "
                              << "Aguardando waypoint do Planejamento." << std::endl;
                    logged_no_route = true;
                }

                boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
                continue;
            }
            else
            {
                logged_no_route = false;
            }

            // ----------------------------------------------------------------
            // 5. CALCULAR ERRO DE POSIÇÃO E ORIENTAÇÃO
            // ----------------------------------------------------------------
            const double dx = static_cast<double>(target_waypoint.x - pos_x);
            const double dy = static_cast<double>(target_waypoint.y - pos_y);

            const double distancia = std::sqrt(dx * dx + dy * dy);

            // Se já estamos muito perto do waypoint, desabilita controle
            if (distancia < EPS_DIST)
            {
                boost::lock_guard<boost::mutex> lock(nav_state.mtx);
                nav_state.u_aceleracao_auto = 0;
                nav_state.u_direcao_auto = 0;
                nav_state.u_valido = false;

                boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
                continue;
            }

            double angulo_desejado_rad = std::atan2(dy, dx);
            double angulo_desejado_deg = angulo_desejado_rad * RAD2DEG;

            double erro_heading = angulo_desejado_deg - static_cast<double>(angulo_atual);
            erro_heading = normalize_angle_deg(erro_heading); // [-180, +180]

            // ----------------------------------------------------------------
            // 6. CONTROLADOR PROPORCIONAL (P puro)
            // ----------------------------------------------------------------

            // 6.1 Direção
            double u_direcao_raw = NavigationControl::Kp_heading * erro_heading;
            double u_direcao_sat = clamp(
                u_direcao_raw,
                static_cast<double>(NavigationControl::STEER_MIN),
                static_cast<double>(NavigationControl::STEER_MAX));
            int u_direcao_auto = static_cast<int>(std::lround(u_direcao_sat));

            // 6.2 Aceleração
            double u_aceleracao_raw = 0.0;
            if (distancia > NavigationControl::SLOWDOWN_DIST)
            {
                u_aceleracao_raw = NavigationControl::Kp_distance * distancia;
            }
            else
            {
                double fator_reducao = distancia / NavigationControl::SLOWDOWN_DIST; // 0..1
                u_aceleracao_raw = NavigationControl::Kp_distance * distancia * fator_reducao;
            }

            double u_aceleracao_sat = clamp(
                u_aceleracao_raw,
                static_cast<double>(NavigationControl::ACCEL_MIN),
                static_cast<double>(NavigationControl::ACCEL_MAX));
            int u_aceleracao_auto = static_cast<int>(std::lround(u_aceleracao_sat));

            // ----------------------------------------------------------------
            // 7. ESCREVER COMANDOS AUTOMÁTICOS
            // ----------------------------------------------------------------
            {
                boost::lock_guard<boost::mutex> lock(nav_state.mtx);
                nav_state.u_aceleracao_auto = u_aceleracao_auto;
                nav_state.u_direcao_auto = u_direcao_auto;
                nav_state.u_valido = true;
            }

            // ----------------------------------------------------------------
            // 8. LOG DE DEBUG (reduzido)
            // ----------------------------------------------------------------
            if (log_counter % 20 == 0)
            {
                std::cout << "[Controle " << id << "] "
                          << "Pos: (" << pos_x << ", " << pos_y << ") | "
                          << "Alvo: (" << target_waypoint.x << ", " << target_waypoint.y << ") | "
                          << "Dist: " << distancia << " | "
                          << "Ang_atual: " << angulo_atual << "° | "
                          << "Ang_desejado: " << angulo_desejado_deg << "° | "
                          << "Erro_heading: " << erro_heading << "° | "
                          << "u_acel: " << u_aceleracao_auto << " | "
                          << "u_dir: " << u_direcao_auto << "°"
                          << std::endl;
            }
            ++log_counter;

            // ----------------------------------------------------------------
            // 9. PRÓXIMO CICLO
            // ----------------------------------------------------------------
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Controle " << id << "] Thread interrompida." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Controle " << id << "] std::exception: " << e.what() << std::endl;
    }

    // Estado neutro ao encerrar
    {
        boost::lock_guard<boost::mutex> lock(nav_state.mtx);
        nav_state.u_aceleracao_auto = 0;
        nav_state.u_direcao_auto = 0;
        nav_state.u_valido = false;
    }

    std::cout << "[Controle " << id << "] Thread de Controle de Navegacao encerrada." << std::endl;
}

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include "controle_de_navegacao.h"
#include "planejamento_de_rota.h"
#include "buffer_circular_compartilhado.h"

// Simula sensores artificialmente
void fake_sensor_thread(std::atomic<bool> &run, SharedCircularBuffer &buf,
                        NavigationControlState &nav)
{
    double x = 0, y = 0, ang = 45;

    while (run)
    {
        // Lê comandos de controle
        int dir_cmd = 0;
        int acel_cmd = 0;
        {
            boost::lock_guard<boost::mutex> lock(nav.mtx);
            dir_cmd = nav.u_direcao_auto;
            acel_cmd = nav.u_aceleracao_auto;
        }

        // Atualiza dinâmica (simplificada)
        ang += dir_cmd * 0.1; // Integra comando de direção
        if (ang > 360)
            ang -= 360;
        if (ang < 0)
            ang += 360;

        double vel = acel_cmd * 0.02; // Velocidade proporcional à aceleração
        x += vel * std::cos(ang * M_PI / 180.0);
        y += vel * std::sin(ang * M_PI / 180.0);

        BufferData data;
        data.i_posicao_x = (int)x;
        data.i_posicao_y = (int)y;
        data.i_angulo_x = (int)ang;
        data.i_temperatura = 75;
        data.i_falha_eletrica = false;
        data.i_falha_hidraulica = false;

        buf.write(data, run);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Monitora comandos
void monitor_thread(std::atomic<bool> &run, NavigationControlState &nav)
{
    int last_a = -999, last_d = -999;
    while (run)
    {
        int a, d;
        bool v;
        {
            boost::lock_guard<boost::mutex> lock(nav.mtx);
            a = nav.u_aceleracao_auto;
            d = nav.u_direcao_auto;
            v = nav.u_valido;
        }

        if (a != last_a || d != last_d)
        {
            std::cout << "[COMANDOS] Acel=" << a << "% Dir=" << d
                      << "° Valido=" << (v ? "SIM" : "NAO") << std::endl;
            last_a = a;
            last_d = d;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// No test_main.cpp, antes do main()
void planejamento_thread_test(int id, int sleep_ms,
                              std::atomic<bool> &running_flag,
                              SharedCircularBuffer &buffer,
                              RouteSharedState &route_state)
{
    // Versão SEM MQTT - apenas atualiza progresso
    while (running_flag)
    {
        BufferData data = buffer.read(id, running_flag);
        if (!running_flag)
            break;

        Waypoint current_wp;
        if (!get_current_waypoint(route_state, current_wp))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            continue;
        }

        double dist = std::sqrt(
            std::pow(current_wp.x - data.i_posicao_x, 2) +
            std::pow(current_wp.y - data.i_posicao_y, 2));

        if (dist < 2.0)
        {
            boost::lock_guard<boost::mutex> lock(route_state.mtx);
            route_state.current_index++;
            if (route_state.current_index >= route_state.waypoints.size())
            {
                route_state.route_completed = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}

int main()
{
    std::cout << "=== TESTE: Planejamento + Controle ===" << std::endl;

    // Estados compartilhados
    std::atomic<bool> running{true};
    SharedCircularBuffer buffer(200, 2); // 2 consumidores
    RouteSharedState route_state;
    LogicSharedState logic_state;
    NavigationControlState nav_state;

    // Configura modo automático
    logic_state.e_automatico = true;
    logic_state.e_defeito = false;

    // Injeta rota: (0,0) -> (100,100)
    auto waypoints = generate_linear_route(0, 0, 100, 100, 20);
    set_new_route(route_state, waypoints);
    std::cout << "Rota injetada: 20 waypoints de (0,0) ate (100,100)" << std::endl;

    // Inicia threads
    std::thread t_sensor(fake_sensor_thread, std::ref(running),
                         std::ref(buffer), std::ref(nav_state));

    std::thread t_plan(planejamento_thread_test, 0, 100, std::ref(running),
                       std::ref(buffer), std::ref(route_state));
    std::thread t_ctrl(controle_thread, 1, 100, std::ref(running),
                       std::ref(buffer), std::ref(route_state),
                       std::ref(logic_state), std::ref(nav_state));
    std::thread t_mon(monitor_thread, std::ref(running), std::ref(nav_state));

    std::cout << "Threads iniciadas. Testando por 10 segundos..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    running = false;
    buffer.notify_all_for_shutdown();

    t_sensor.join();
    t_plan.join();
    t_ctrl.join();
    t_mon.join();

    std::cout << "Teste concluido." << std::endl;
    return 0;
}

/**
 * @file main.cpp
 * @brief Ponto de entrada principal do Sistema de Controle de Caminhão Autônomo
 * @version 2.0
 * @date 2025-12-03
 *
 * Este programa inicializa e gerencia todas as tarefas concorrentes do sistema:
 * - Tratamento de Sensores (filtro + buffer circular)
 * - Planejamento de Rota (waypoint management + MQTT)
 * - Controle de Navegação (piloto automático)
 * - Lógica de Comando (arbitragem manual/automático + MQTT atuadores)
 * - Monitoramento de Falhas (detecção + broadcast de eventos)
 * - Coletor de Dados (logging + telemetria)
 */

#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

// Boost Thread Library
#include <boost/thread.hpp>

// Configurações do sistema
#include "config.hpp"

// Headers das tarefas
#include "tratamento_sensores.h"
#include "planejamento_de_rota.h"
#include "controle_de_navegacao.h"
#include "logica_de_comando.h"
#include "monitoramento_de_falhas.h"
#include "coletor_de_dados.h"

// Estruturas compartilhadas
#include "buffer_circular_compartilhado.h"
#include "evento_de_falhas.h"

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

/// Flag atômica global para shutdown coordenado de todas as threads
std::atomic<bool> g_running(true);

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

/**
 * @brief Manipulador de sinais para encerramento gracioso (Ctrl+C / SIGTERM).
 *
 * Quando o usuário pressiona Ctrl+C ou o sistema envia SIGTERM:
 * 1. Seta g_running = false (todas as threads verificam esse flag)
 * 2. Permite que as threads finalizem suas tarefas atuais
 * 3. main() então chama interrupt() e join() para limpeza final
 */
void signal_handler(int signum)
{
    std::cout << "\n[Main] Sinal " << signum << " recebido. Iniciando shutdown..." << std::endl;
    g_running = false;
}

// ============================================================================
// FUNÇÃO PRINCIPAL
// ============================================================================

int main()
{
    // ========================================================================
    // BANNER DE INICIALIZAÇÃO
    // ========================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "  " << project_name << " v" << project_version << std::endl;
    std::cout << "  Sistema de Controle Autônomo de Mineração" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "[Main] Inicializando componentes do sistema..." << std::endl;

    // ========================================================================
    // REGISTRAR MANIPULADORES DE SINAIS
    // ========================================================================
    signal(SIGINT, signal_handler);  // Ctrl+C
    signal(SIGTERM, signal_handler); // kill <pid>

    std::cout << "[Main] Manipuladores de sinal registrados (SIGINT, SIGTERM)." << std::endl;

    // ========================================================================
    // CRIAR ESTRUTURAS COMPARTILHADAS
    // ========================================================================

    std::cout << "[Main] Criando estruturas compartilhadas..." << std::endl;

    // Buffer Circular Multi-Consumidor (sensores tratados)
    // Capacidade: 200 posições, Consumidores: 5 (Planejamento, Controle, Lógica, Coletor, reserva)
    SharedCircularBuffer buffer(BUFF_CAPACIDADE, BUFF_CONSUMIDORES);
    std::cout << "[Main]   ✓ SharedCircularBuffer: capacidade=" << BUFF_CAPACIDADE
              << ", consumidores=" << BUFF_CONSUMIDORES << std::endl;

    // Barramento de Eventos de Falha (broadcast para 3 consumidores)
    FaultEventBus event_bus;
    std::cout << "[Main]   ✓ FaultEventBus: 3 filas (Lógica, Controle, Coletor)" << std::endl;

    // Estado Compartilhado de Rota (waypoints + progresso)
    RouteSharedState route_state;
    std::cout << "[Main]   ✓ RouteSharedState: gerenciamento de rota e waypoints" << std::endl;

    // Estado Compartilhado de Lógica de Comando (modo automático/manual + defeito)
    LogicSharedState logic_state;
    std::cout << "[Main]   ✓ LogicSharedState: modo operação e flags de defeito" << std::endl;

    // Estado Compartilhado de Controle de Navegação (comandos automáticos)
    NavigationControlState nav_state;
    std::cout << "[Main]   ✓ NavigationControlState: comandos do piloto automático" << std::endl;

    // ========================================================================
    // CRIAR E INICIAR THREADS
    // ========================================================================

    std::cout << "\n[Main] Iniciando threads do sistema..." << std::endl;

    try
    {
        // --------------------------------------------------------------------
        // THREAD H: Tratamento de Sensores (Produtor do Buffer)
        // --------------------------------------------------------------------
        boost::thread thread_h(
            tratamento_thread,
            ID_TRATAMENTO,
            SLEEP_MS_TRATAMENTO,
            std::ref(g_running),
            std::ref(buffer));
        std::cout << "[Main]   ✓ Thread H (Tratamento Sensores) iniciada [ID="
                  << ID_TRATAMENTO << ", período=" << SLEEP_MS_TRATAMENTO << "ms]" << std::endl;

        // --------------------------------------------------------------------
        // THREAD G: Planejamento de Rota (Consumidor do Buffer)
        // --------------------------------------------------------------------
        boost::thread thread_g(
            planejamento_thread,
            ID_PLANEJAMENTO,
            SLEEP_MS_PLANEJAMENTO,
            std::ref(g_running),
            std::ref(buffer),
            std::ref(route_state));
        std::cout << "[Main]   ✓ Thread G (Planejamento Rota) iniciada [ID="
                  << ID_PLANEJAMENTO << ", período=" << SLEEP_MS_PLANEJAMENTO << "ms]" << std::endl;

        // --------------------------------------------------------------------
        // THREAD D: Controle de Navegação (Consumidor do Buffer + RouteState)
        // --------------------------------------------------------------------
        boost::thread thread_d(
            controle_thread,
            ID_CONTROLE,
            SLEEP_MS_CONTROLE,
            std::ref(g_running),
            std::ref(buffer),
            std::ref(route_state),
            std::ref(logic_state),
            std::ref(nav_state));
        std::cout << "[Main]   ✓ Thread D (Controle Navegação) iniciada [ID="
                  << ID_CONTROLE << ", período=" << SLEEP_MS_CONTROLE << "ms]" << std::endl;

        // --------------------------------------------------------------------
        // THREAD F: Monitoramento de Falhas (Produtor do EventBus)
        // --------------------------------------------------------------------
        boost::thread thread_f(
            monitoramento_thread,
            ID_MONITORAMENTO,
            SLEEP_MS_MONITORAMENTO,
            std::ref(g_running),
            std::ref(event_bus));
        std::cout << "[Main]   ✓ Thread F (Monitoramento Falhas) iniciada [ID="
                  << ID_MONITORAMENTO << ", período=" << SLEEP_MS_MONITORAMENTO << "ms]" << std::endl;

        // --------------------------------------------------------------------
        // THREAD E: Lógica de Comando (Consumidor EventBus + NavState + Interface MQTT)
        // --------------------------------------------------------------------
        boost::thread thread_e(
            comando_thread,
            ID_COMANDO,
            SLEEP_MS_COMANDO,
            std::ref(g_running),
            std::ref(buffer),
            std::ref(event_bus),
            std::ref(nav_state),
            std::ref(logic_state));
        std::cout << "[Main]   ✓ Thread E (Lógica Comando) iniciada [ID="
                  << ID_COMANDO << ", período=" << SLEEP_MS_COMANDO << "ms]" << std::endl;

        // --------------------------------------------------------------------
        // THREAD C: Coletor de Dados (Consumidor do Buffer + EventBus)
        // --------------------------------------------------------------------
        boost::thread thread_c(
            coletor_thread,
            ID_COLETOR,
            SLEEP_MS_COLETOR,
            std::ref(g_running),
            std::ref(buffer),
            std::ref(event_bus));
        std::cout << "[Main]   ✓ Thread C (Coletor Dados) iniciada [ID="
                  << ID_COLETOR << ", período=" << SLEEP_MS_COLETOR << "ms]" << std::endl;

        // ====================================================================
        // LOOP DE MONITORAMENTO PRINCIPAL
        // ====================================================================
        std::cout << "\n[Main] Todas as threads iniciadas com sucesso!" << std::endl;
        std::cout << "[Main] Sistema operacional. Pressione Ctrl+C para encerrar.\n"
                  << std::endl;

        // Loop de monitoramento (verifica g_running a cada 100ms)
        while (g_running.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // ====================================================================
        // SHUTDOWN COORDENADO
        // ====================================================================
        std::cout << "\n[Main] Sinal de shutdown detectado. Encerrando threads..." << std::endl;

        // Notifica estruturas compartilhadas para desbloqueio de threads
        std::cout << "[Main] Notificando estruturas compartilhadas para shutdown..." << std::endl;
        buffer.notify_all_for_shutdown();
        event_bus.notify_all_for_shutdown();

        // Interrompe todas as threads (envia boost::thread_interrupted)
        std::cout << "[Main] Interrompendo threads..." << std::endl;
        thread_h.interrupt();
        thread_g.interrupt();
        thread_d.interrupt();
        thread_f.interrupt();
        thread_e.interrupt();
        thread_c.interrupt();

        // Aguarda finalização de todas as threads
        std::cout << "[Main] Aguardando finalização das threads..." << std::endl;
        thread_h.join();
        std::cout << "[Main]   ✓ Thread H (Tratamento) encerrada" << std::endl;

        thread_g.join();
        std::cout << "[Main]   ✓ Thread G (Planejamento) encerrada" << std::endl;

        thread_d.join();
        std::cout << "[Main]   ✓ Thread D (Controle) encerrada" << std::endl;

        thread_f.join();
        std::cout << "[Main]   ✓ Thread F (Monitoramento) encerrada" << std::endl;

        thread_e.join();
        std::cout << "[Main]   ✓ Thread E (Lógica) encerrada" << std::endl;

        thread_c.join();
        std::cout << "[Main]   ✓ Thread C (Coletor) encerrada" << std::endl;

        // ====================================================================
        // FINALIZAÇÃO
        // ====================================================================
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Sistema encerrado com sucesso!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n[Main] ERRO CRÍTICO durante execução: " << e.what() << std::endl;

        // Tenta encerrar graciosamente mesmo em caso de erro
        g_running = false;
        buffer.notify_all_for_shutdown();
        event_bus.notify_all_for_shutdown();

        return 1;
    }
}

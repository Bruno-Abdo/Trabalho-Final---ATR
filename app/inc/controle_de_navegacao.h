#ifndef CONTROLE_DE_NAVEGACAO_H
#define CONTROLE_DE_NAVEGACAO_H

#include <atomic>
#include <boost/thread.hpp>

#include "buffer_circular_compartilhado.h"
#include "planejamento_de_rota.h"

/**
 * @file controle_de_navegacao.h
 * @brief Tarefa de Controle de Navegação (Piloto Automático)
 *
 * Responsabilidades:
 * - Ler estado do caminhão (posição, ângulo) do SharedCircularBuffer
 * - Ler waypoint atual do RouteSharedState
 * - Ler modo de operação (automático/manual, defeito) do LogicSharedState
 * - Calcular comandos automáticos (aceleração, direção) com controlador P
 * - Escrever comandos em NavigationControlState para a Lógica de Comando
 */

// ============================================================================
// ESTADO COMPARTILHADO: Comandos Automáticos do Controle de Navegação
// ============================================================================

/**
 * @brief Comandos automáticos gerados pelo Controle de Navegação.
 *
 * Esta estrutura é:
 * - Escrita APENAS pela thread de Controle de Navegação (controle_thread)
 * - Lida pela thread de Lógica de Comando (comando_thread)
 *
 * A Lógica de Comando decide se usa estes valores (modo automático) ou
 * comandos manuais recebidos da Interface Local.
 */
struct NavigationControlState
{
    /// Comando automático de aceleração [-100, +100]
    /// Negativo = freio/ré, Positivo = aceleração
    int u_aceleracao_auto{0};

    /// Comando automático de direção [-30, +30] graus
    /// Negativo = esquerda, Positivo = direita
    int u_direcao_auto{0};

    /// Flag indicando se os comandos automáticos são válidos
    /// true = modo automático ativo e controlador gerando comandos
    /// false = sem rota, defeito ativo, ou modo manual
    bool u_valido{false};

    /// Mutex para acesso thread-safe (Controle escreve, Lógica lê)
    mutable boost::mutex mtx;
};

// ============================================================================
// ESTADO COMPARTILHADO: Lógica de Comando (Esqueleto)
// ============================================================================

/**
 * @brief Estado de operação definido pela Lógica de Comando.
 *
 * Esta estrutura é:
 * - Escrita pela thread de Lógica de Comando (comando_thread)
 * - Lida pelo Controle de Navegação e possivelmente por outras tarefas
 *
 * NOTA: Esta é uma versão esqueleto. Será expandida quando implementarmos
 * a tarefa Lógica de Comando completa (comando_thread).
 */
struct LogicSharedState
{
    /// true = modo automático ativo (usar comandos do Controle de Navegação)
    /// false = modo manual (usar comandos da Interface Local)
    bool e_automatico{false};

    /// true = defeito ativo (desabilita controle automático, protocolo de segurança)
    /// false = operação normal
    bool e_defeito{false};

    /// Mutex para acesso thread-safe
    mutable boost::mutex mtx;
};

// ============================================================================
// PARÂMETROS DO CONTROLADOR (Ganhos e Saturações)
// ============================================================================

namespace NavigationControl
{
    /// Ganho proporcional do controlador de heading (direção)
    /// Quanto maior, mais agressiva a correção de direção
    constexpr double Kp_heading = 0.5; // [graus_comando / grau_erro]

    /// Ganho proporcional da aceleração em função da distância
    /// aceleracao = Kp_distance * distancia (saturado)
    constexpr double Kp_distance = 3.0; // [% / metro]

    /// Distância de desaceleração [metros]
    /// Quando dist < SLOWDOWN_DIST, reduz aceleração para evitar overshoot
    constexpr double SLOWDOWN_DIST = 5.0;

    /// Saturação de aceleração automática [-100, +100] %
    constexpr int ACCEL_MIN = -100;
    constexpr int ACCEL_MAX = 100;

    /// Saturação de direção automática [-30, +30] graus
    /// (Mais conservador que o limite físico do simulador de ±180°)
    constexpr int STEER_MIN = -30;
    constexpr int STEER_MAX = 30;
}

// ============================================================================
// ASSINATURA DA THREAD DE CONTROLE DE NAVEGAÇÃO
// ============================================================================

/**
 * @brief Thread de Controle de Navegação (Piloto Automático).
 *
 * Funcionamento (loop principal):
 * 1. Lê posição/ângulo atuais do SharedCircularBuffer
 * 2. Lê modo de operação (e_automatico, e_defeito) do LogicSharedState
 * 3. Se não automático ou em defeito:
 *    - Prepara a transferência suave (ajusta estados internos do controlador)
 *    - Marca u_valido = false
 * 4. Se automático e sem defeito:
 *    - Lê waypoint atual do RouteSharedState
 *    - Calcula erro de heading (ângulo desejado - ângulo atual)
 *    - Calcula distância até o waypoint
 *    - Gera u_aceleracao_auto (P na distância, com desaceleração perto do alvo)
 *    - Gera u_direcao_auto (P no heading)
 *    - Satura saídas e marca u_valido = true
 * 5. Escreve NavigationControlState
 * 6. Dorme por sleep_ms antes do próximo ciclo
 *
 * @param id             Identificador lógico da tarefa (ex: ID_CONTROLE)
 * @param sleep_ms       Período da tarefa em milissegundos (ex: SLEEP_MS_CONTROLE)
 * @param running_flag   Flag global de execução (false => shutdown)
 * @param buffer         Buffer circular com dados tratados dos sensores
 * @param route_state    Estado compartilhado da rota (waypoints e progresso)
 * @param logic_state    Estado de operação (automático/manual, defeito)
 * @param nav_state      Estado de saída do controlador (comandos automáticos)
 */
void controle_thread(int id,
                     int sleep_ms,
                     std::atomic<bool> &running_flag,
                     SharedCircularBuffer &buffer,
                     RouteSharedState &route_state,
                     LogicSharedState &logic_state,
                     NavigationControlState &nav_state);

#endif // CONTROLE_DE_NAVEGACAO_H

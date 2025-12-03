#pragma once

#include <string>
#include <deque>
#include <array>
#include <atomic>
#include <iostream>

#include <boost/thread.hpp>

/**
 * @brief Tipos de eventos de falha/alerta gerados pelo Monitoramento de Falhas.
 */
enum class FaultEventType
{
    OvertemperatureAlert, ///< T > 95°C
    OvertemperatureFault, ///< T > 120°C
    ElectricalFault,      ///< i_falha_eletrica == true
    HydraulicFault,       ///< i_falha_hidraulica == true
    FaultCleared          ///< Retorno ao estado normal (opcional)
};

/**
 * @brief Evento de falha completo com contexto.
 */
struct FaultEvent
{
    FaultEventType type;
    double timestamp;
    std::string truck_id;

    // Contexto de sensores
    int temperatura;
    bool falha_eletrica;
    bool falha_hidraulica;

    std::string description;

    // Construtor padrão
    FaultEvent()
        : type(FaultEventType::OvertemperatureAlert),
          timestamp(0.0),
          truck_id(""),
          temperatura(0),
          falha_eletrica(false),
          falha_hidraulica(false),
          description("")
    {
    }
};

/**
 * @brief Barramento de eventos com filas por consumidor (broadcast).
 */
class FaultEventBus
{
public:
    enum class Consumer : std::size_t
    {
        Logica = 0,
        Controle = 1,
        Coletor = 2,
        Count = 3
    };

    static constexpr std::size_t MAX_QUEUE_SIZE = 1000; // Proteção

    FaultEventBus() = default;
    ~FaultEventBus() = default;

    /**
     * @brief Insere evento em TODAS as filas de consumidores.
     */
    void push(const FaultEvent &ev)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);

        for (auto &queue : m_queues)
        {
            // Proteção contra overflow
            if (queue.size() >= MAX_QUEUE_SIZE)
            {
                std::cerr << "[FaultEventBus] AVISO: Fila atingiu limite de "
                          << MAX_QUEUE_SIZE << " eventos. Descartando mais antigo.\n";
                queue.pop_front();
            }
            queue.push_back(ev);
        }

        lock.unlock();
        m_cv.notify_all();
    }

    /**
     * @brief Leitura bloqueante com suporte a shutdown.
     */
    bool wait_and_pop(Consumer consumer, FaultEvent &out, std::atomic<bool> &running)
    {
        const auto idx = static_cast<std::size_t>(consumer);
        boost::unique_lock<boost::mutex> lock(m_mutex);

        while (m_queues[idx].empty() && running.load())
        {
            m_cv.wait(lock);
        }

        if (!running.load())
        {
            return false;
        }

        out = m_queues[idx].front();
        m_queues[idx].pop_front();
        return true;
    }

    /**
     * @brief Leitura não bloqueante.
     */
    bool try_pop(Consumer consumer, FaultEvent &out)
    {
        const auto idx = static_cast<std::size_t>(consumer);
        boost::unique_lock<boost::mutex> lock(m_mutex);

        if (m_queues[idx].empty())
        {
            return false;
        }

        out = m_queues[idx].front();
        m_queues[idx].pop_front();
        return true;
    }

    /**
     * @brief Notifica para shutdown limpo.
     */
    void notify_all_for_shutdown()
    {
        m_cv.notify_all();
    }

private:
    using Queue = std::deque<FaultEvent>;
    std::array<Queue, static_cast<std::size_t>(Consumer::Count)> m_queues{};

    boost::mutex m_mutex;
    boost::condition_variable m_cv;
};

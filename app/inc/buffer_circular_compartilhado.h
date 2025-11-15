#pragma once

#include "config.hpp"
#include <vector>
#include <boost/thread.hpp> // Inclui mutex, condition_variable, etc.

class SharedCircularBuffer
{
public:
    /**
     * @param capacity O tamanho total do buffer (ex: 200).
     * @param num_consumers O número de threads consumidoras (ex: 5).
     */
    SharedCircularBuffer(size_t capacity, size_t num_consumers)
        : m_capacity(capacity),
          m_write_head(0),
          m_consumer_read_heads(num_consumers, 0) // Todos começam em 0
    {
        m_buffer.resize(m_capacity);
    }

    /**
     * @brief Chamado por QUALQUER thread Produtora.
     * Bloqueia se o buffer estiver cheio (se o consumidor mais lento
     * ainda não leu a posição que será sobrescrita).
     */
    void write(const BufferData &data, std::atomic<bool> &running);

    /**
     * @brief Chamado por QUALQUER thread Consumidora.
     * @param consumer_id O ID único da thread (0, 1, 2...).
     * @return BufferData O dado lido.
     */
    BufferData read(size_t consumer_id, std::atomic<bool> &running);

    /**
     * @brief Acorda todas as threads que estão esperando (para shutdown).
     */
    void notify_all_for_shutdown();

private:
    // --- Helpers de Prédicado (para CVs) ---
    // Retorna true se o buffer estiver "cheio" para o produtor
    bool is_full() const;

    // Retorna true se o buffer estiver "vazio" para este consumidor
    bool is_empty(size_t consumer_id) const;

    // --- Estado do Buffer ---
    size_t m_capacity;
    std::vector<BufferData> m_buffer;
    size_t m_write_head;

    // --- O PONTO CHAVE: Rastreia cada consumidor ---
    std::vector<size_t> m_consumer_read_heads;

    // --- Sincronização Boost ---
    boost::mutex m_mutex;
    boost::condition_variable m_can_write_cv; // Produtores esperam aqui
    boost::condition_variable m_can_read_cv;  // Consumidores esperam aqui
};

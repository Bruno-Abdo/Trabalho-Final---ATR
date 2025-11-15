#include "buffer_circular_compartilhado.h"
#include <algorithm> // Para std::min_element

// --- Predicados (Chamados DEPOIS de travar o mutex) ---

bool SharedCircularBuffer::is_full() const
{
    // 1. Encontra o ponteiro do consumidor MAIS LENTO
    // Esta é a implementação da sua restrição!
    size_t slowest_consumer = *std::min_element(
        m_consumer_read_heads.begin(), m_consumer_read_heads.end());

    // 2. O buffer está "cheio" se o próximo write_head for
    //    colidir com o ponteiro do consumidor mais lento.
    return (m_write_head + 1) % m_capacity == slowest_consumer;
}

bool SharedCircularBuffer::is_empty(size_t consumer_id) const
{
    // O buffer está "vazio" para ESTE consumidor se o seu ponteiro
    // de leitura alcançou o ponteiro de escrita.
    return m_consumer_read_heads[consumer_id] == m_write_head;
}

// --- Funções Públicas ---

void SharedCircularBuffer::write(const BufferData &data, std::atomic<bool> &running)
{
    // 1. Trava o buffer (só uma thread pode escrever OU ler por vez)
    boost::unique_lock<boost::mutex> lock(m_mutex);

    // 2. Espera (liberando o lock) até que o buffer NÃO esteja cheio
    //    O 'while' previne "spurious wakeups"
    while (is_full() && running)
    {
        m_can_write_cv.wait(lock);
    }
    if (!running)
        return;

    // 3. Escreve o dado e avança o ponteiro
    m_buffer[m_write_head] = data;
    m_write_head = (m_write_head + 1) % m_capacity;

    // 4. Libera o lock (automaticamente) e acorda TODOS os consumidores
    lock.unlock();
    m_can_read_cv.notify_all();
}

BufferData SharedCircularBuffer::read(size_t consumer_id, std::atomic<bool> &running)
{
    // 1. Trava o buffer
    boost::unique_lock<boost::mutex> lock(m_mutex);

    // 2. Espera (liberando o lock) até que haja dados para ESTE consumidor
    while (is_empty(consumer_id) && running)
    {
        m_can_read_cv.wait(lock);
    }
    if (!running)
        return {}; // Retorna dado vazio no shutdown

    // 3. Lê o dado e avança o ponteiro DESTE consumidor
    BufferData data = m_buffer[m_consumer_read_heads[consumer_id]];
    m_consumer_read_heads[consumer_id] =
        (m_consumer_read_heads[consumer_id] + 1) % m_capacity;

    // 4. Libera o lock (automaticamente) e acorda UM produtor
    lock.unlock();
    m_can_write_cv.notify_one();

    return data;
}

void SharedCircularBuffer::notify_all_for_shutdown()
{
    m_can_write_cv.notify_all();
    m_can_read_cv.notify_all();
}

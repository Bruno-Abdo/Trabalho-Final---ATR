/**
 * @file tratamento_sensores.cpp
 * @brief Tarefa de Tratamento de Sensores com cliente MQTT síncrono
 * @version 1.0
 * @date 2025-11-30
 */

#include <iostream>
#include <optional>
#include <string>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <nlohmann/json.hpp>
#include <mqtt/client.h>

#include "tratamento_sensores.h"
#include "config.hpp"

namespace
{
    using json = nlohmann::json;

    /**
     * @brief Filtro de média móvel
     *
     * @tparam T Tipo numérico (int, double, etc.)
     */
    template <typename T>
    class MovingAverageFilter
    {
    public:
        /**
         * @brief Construtor que valida ordem do filtro
         * @param window_size Ordem M do filtro (número de amostras na janela)
         * @throws std::invalid_argument se window_size == 0
         */
        explicit MovingAverageFilter(std::size_t window_size)
            : m_window_size(window_size),
              m_values(window_size, 0),
              m_count(0),
              m_index(0),
              m_sum(0)
        {
            if (window_size == 0)
            {
                throw std::invalid_argument(
                    "[MovingAverageFilter] window_size não pode ser zero. "
                    "Verifique FILTER_ORDER_M em config.hpp.");
            }
        }
        /**
         * @brief Adiciona nova amostra e retorna média filtrada
         * @param new_value Valor bruto (não filtrado)
         * @return Média das últimas M amostras
         */
        T update(T new_value)
        {
            if (m_window_size == 0)
            {
                // Se a janela for 0 (config incorreta), não filtra
                return new_value;
            }

            std::size_t slot = (m_count < m_window_size) ? m_count : m_index;
            std::size_t n = (m_count < m_window_size) ? (m_count + 1) : m_window_size;

            // Remove valor antigo daquele slot da soma
            m_sum -= m_values[slot];

            // Insere novo valor
            m_values[slot] = new_value;
            m_sum += new_value;

            // Atualiza contadores da janela
            if (m_count < m_window_size)
            {
                ++m_count;
            }
            else
            {
                m_index = (m_index + 1) % m_window_size;
            }

            // Média simples (inteira)
            return static_cast<T>(m_sum / static_cast<long long>(n));
        }

    private:
        std::size_t m_window_size; ///< Ordem M do filtro
        std::vector<T> m_values;   ///< Buffer circular pré-alocado
        std::size_t m_count;       ///< Total de amostras já vistas
        std::size_t m_index;       ///< Índice de escrita (após warm-up)
        long long m_sum;           ///< Soma incremental (evita recalcular)
    };

    /**
     * @brief Encapsula os 4 filtros de média móvel das variáveis de sensor
     *
     * Facilita extensão (basta adicionar novo membro para novo sensor)
     */
    struct SensorFilters
    {
        MovingAverageFilter<int> pos_x{FILTER_ORDER_M};
        MovingAverageFilter<int> pos_y{FILTER_ORDER_M};
        MovingAverageFilter<int> ang_x{FILTER_ORDER_M};
        MovingAverageFilter<int> temp{FILTER_ORDER_M};
    };

    /**
     * @brief Parseia payload JSON do MQTT para estrutura BufferData
     *
     * @param payload String JSON recebida do MQTT
     * @param out Estrutura BufferData a ser preenchida (valores brutos)
     * @return true se parsing bem-sucedido, false caso contrário
     */
    bool parse_sensor_message(const std::string &payload, BufferData &out)
    {
        try
        {
            auto j = json::parse(payload);

            // truck_id e timestamp (se não vierem, usamos valores padrão)
            if (j.contains("truck_id") && j["truck_id"].is_string())
            {
                out.truck_id = j["truck_id"].get<std::string>();
            }

            if (j.contains("timestamp") && (j["timestamp"].is_number_float() || j["timestamp"].is_number_integer()))
            {
                out.timestamp = j["timestamp"].get<double>();
            }

            // Objeto "sensors"
            if (!j.contains("sensors") || !j["sensors"].is_object())
            {
                std::cerr << "[Tratamento] JSON sem objeto 'sensors'. Ignorando mensagem.\n";
                return false;
            }

            const auto &js = j["sensors"];

            // Todas as variáveis de sensores (valores brutos por enquanto; o filtro virá depois)
            if (js.contains("i_posicao_x") && js["i_posicao_x"].is_number_integer())
                out.i_posicao_x = js["i_posicao_x"].get<int>();

            if (js.contains("i_posicao_y") && js["i_posicao_y"].is_number_integer())
                out.i_posicao_y = js["i_posicao_y"].get<int>();

            if (js.contains("i_angulo_x") && js["i_angulo_x"].is_number_integer())
                out.i_angulo_x = js["i_angulo_x"].get<int>();

            if (js.contains("i_temperatura") && js["i_temperatura"].is_number_integer())
                out.i_temperatura = js["i_temperatura"].get<int>();

            if (js.contains("i_falha_eletrica") && js["i_falha_eletrica"].is_boolean())
                out.i_falha_eletrica = js["i_falha_eletrica"].get<bool>();

            if (js.contains("i_falha_hidraulica") && js["i_falha_hidraulica"].is_boolean())
                out.i_falha_hidraulica = js["i_falha_hidraulica"].get<bool>();

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Tratamento] Erro ao parsear JSON: " << e.what() << "\n";
            return false;
        }
    }
} // namespace

void tratamento_thread(int id,
                       int sleep_ms,
                       std::atomic<bool> &running_flag,
                       SharedCircularBuffer &buffer)
{
    std::cout << "[Tratamento] Thread " << id << " is starting." << std::endl;

    try
    {
        std::cout << "[Tratamento] Connecting to MQTT broker at " << MQTT_URL << std::endl;

        // 1) Cria cliente MQTT síncrono
        mqtt::client client(MQTT_URL, "tratamento_client");
        SensorFilters filters;

        mqtt::connect_options conn_opts;
        conn_opts.set_clean_session(true);

        client.connect(conn_opts);
        std::cout << "[Tratamento] Connected to broker. Subscribing to topic '"
                  << MQTT_TOPIC_SENSORS << "'\n";

        // 2) Subscribe no tópico dos sensores
        client.subscribe(MQTT_TOPIC_SENSORS, MQTT_DEFAULT_QOS);

        // 3) Loop principal: polling + parsing
        while (running_flag.load())
        {
            // Permite interrupção via boost::thread::interrupt()
            boost::this_thread::interruption_point();

            mqtt::const_message_ptr msg;
            // Espera até 'sleep_ms' por uma mensagem nova
            bool got_msg = client.try_consume_message_for(
                &msg,
                std::chrono::milliseconds(sleep_ms));

            if (!got_msg)
            {
                // Sem mensagem nova nesse período -> volta pro topo do loop
                continue;
            }

            if (!msg)
            {
                // Mensagem nula (caso raro) -> ignora
                continue;
            }

            const std::string payload = msg->get_payload_str();
            BufferData data; // começa com valores padrão

            if (!parse_sensor_message(payload, data))
            {
                // JSON inválido -> apenas loga o erro e ignora a amostra
                continue;
            }

            // 4) Aplica filtro de média móvel nas variáveis numéricas
            data.i_posicao_x = filters.pos_x.update(data.i_posicao_x);
            data.i_posicao_y = filters.pos_y.update(data.i_posicao_y);
            data.i_angulo_x = filters.ang_x.update(data.i_angulo_x);
            data.i_temperatura = filters.temp.update(data.i_temperatura);

            // 5) Escreve a amostra TRATADA no buffer circular compartilhado
            buffer.write(data, running_flag);

            // (Opcional) Log reduzido de debug:
            // std::cout << "[Tratamento] Escreveu amostra filtrada: "
            //           << "id=" << data.truck_id
            //           << ", ts=" << data.timestamp
            //           << ", x=" << data.i_posicao_x
            //           << ", y=" << data.i_posicao_y
            //           << ", ang=" << data.i_angulo_x
            //           << ", T=" << data.i_temperatura
            //           << std::endl;
        }

        // 6) Desconecta ao sair do loop
        std::cout << "[Tratamento] Disconnecting MQTT client..." << std::endl;
        client.disconnect();
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Tratamento] Thread " << id << " was interrupted." << std::endl;
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "[Tratamento] MQTT exception: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Tratamento] std::exception in tratamento_thread: " << e.what() << std::endl;
    }

    std::cout << "[Tratamento] Thread " << id << " is stopping." << std::endl;
}

/**
 * @file coletor_de_dados.cpp
 * @brief Tarefa de Coletor de Dados: estados + eventos de falha
 * @version 1.1
 * @date 2025-12-03
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <boost/thread.hpp>
#include <iomanip>
#include <sstream>
#include <ctime>

#include "coletor_de_dados.h"
#include "evento_de_falhas.h"

namespace
{
    // =========================================================================
    // CONFIGURAÇÃO DE ARQUIVOS
    // =========================================================================
    const std::string kLogDir = "logs";
    const std::string kLogFile = "logs/truck_data.csv";
    const std::string kEventsLogFile = "logs/truck_events.csv";

    // =========================================================================
    // FUNÇÕES AUXILIARES DE FORMATAÇÃO
    // =========================================================================

    /**
     * @brief Converte epoch em segundos (UTC) para string ISO 8601 em BRT (UTC-3)
     */
    std::string format_timestamp_brt(double ts_seconds)
    {
        double ts_brt = ts_seconds - 3.0 * 3600.0;
        std::time_t sec = static_cast<std::time_t>(ts_brt);

        double frac = ts_brt - static_cast<double>(sec);
        if (frac < 0.0)
        {
            sec -= 1;
            frac += 1.0;
        }
        long millis = static_cast<long>(frac * 1000.0 + 0.5);

        std::tm tm_brt{};
#if defined(_WIN32)
        gmtime_s(&tm_brt, &sec);
#else
        gmtime_r(&sec, &tm_brt);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_brt, "%Y-%m-%dT%H:%M:%S");
        oss << "." << std::setw(3) << std::setfill('0') << millis << "-03:00";

        return oss.str();
    }

    /**
     * @brief Converte FaultEventType em string para logging
     *
     * Usa convenção UPPER_SNAKE_CASE para compatibilidade com ferramentas de análise de logs
     */
    std::string fault_event_type_to_string(FaultEventType t)
    {
        switch (t)
        {
        case FaultEventType::OvertemperatureAlert:
            return "OVERTEMPERATURE_ALERT";
        case FaultEventType::OvertemperatureFault:
            return "OVERTEMPERATURE_FAULT";
        case FaultEventType::ElectricalFault:
            return "ELECTRICAL_FAULT";
        case FaultEventType::HydraulicFault:
            return "HYDRAULIC_FAULT";
        case FaultEventType::FaultCleared:
            return "FAULT_CLEARED";
        default:
            return "UNKNOWN";
        }
    }

    // =========================================================================
    // GERENCIAMENTO DE ARQUIVOS CSV
    // =========================================================================

    bool ensure_log_directory()
    {
        try
        {
            if (!std::filesystem::exists(kLogDir))
            {
                std::filesystem::create_directories(kLogDir);
            }
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Coletor] Erro ao criar diretório de logs: "
                      << e.what() << std::endl;
            return false;
        }
    }

    bool open_csv_log(std::ofstream &ofs)
    {
        if (!ensure_log_directory())
        {
            return false;
        }

        bool needs_header = false;
        if (!std::filesystem::exists(kLogFile))
        {
            needs_header = true;
        }
        else
        {
            auto sz = std::filesystem::file_size(kLogFile);
            if (sz == 0)
                needs_header = true;
        }

        ofs.open(kLogFile, std::ios::out | std::ios::app);
        if (!ofs.is_open())
        {
            std::cerr << "[Coletor] Não foi possível abrir o arquivo CSV: "
                      << kLogFile << std::endl;
            return false;
        }

        if (needs_header)
        {
            ofs << "timestamp,truck_id,"
                << "e_defeito,e_automatico,"
                << "i_posicao_x,i_posicao_y,i_angulo_x,i_temperatura,"
                << "i_falha_eletrica,i_falha_hidraulica"
                << "\n";
            ofs.flush();
        }
        return true;
    }

    bool open_events_log(std::ofstream &ofs)
    {
        if (!ensure_log_directory())
        {
            return false;
        }

        bool needs_header = false;
        if (!std::filesystem::exists(kEventsLogFile))
        {
            needs_header = true;
        }
        else
        {
            auto sz = std::filesystem::file_size(kEventsLogFile);
            if (sz == 0)
                needs_header = true;
        }

        ofs.open(kEventsLogFile, std::ios::out | std::ios::app);
        if (!ofs.is_open())
        {
            std::cerr << "[Coletor] Não foi possível abrir o arquivo CSV de eventos: "
                      << kEventsLogFile << std::endl;
            return false;
        }

        if (needs_header)
        {
            ofs << "timestamp,truck_id,event_type,"
                << "temperatura,falha_eletrica,falha_hidraulica,description"
                << "\n";
            ofs.flush();
        }
        return true;
    }

    void write_csv_line(std::ofstream &ofs, const BufferData &data)
    {
        ofs << format_timestamp_brt(data.timestamp) << ","
            << data.truck_id << ","
            << (data.e_defeito ? 1 : 0) << ","
            << (data.e_automatico ? 1 : 0) << ","
            << data.i_posicao_x << ","
            << data.i_posicao_y << ","
            << data.i_angulo_x << ","
            << data.i_temperatura << ","
            << (data.i_falha_eletrica ? 1 : 0) << ","
            << (data.i_falha_hidraulica ? 1 : 0)
            << "\n";

        ofs.flush();
    }

    void write_event_csv_line(std::ofstream &ofs, const FaultEvent &ev)
    {
        ofs << format_timestamp_brt(ev.timestamp) << ","
            << ev.truck_id << ","
            << fault_event_type_to_string(ev.type) << ","
            << ev.temperatura << ","
            << (ev.falha_eletrica ? 1 : 0) << ","
            << (ev.falha_hidraulica ? 1 : 0) << ","
            << "\"" << ev.description << "\""
            << "\n";

        ofs.flush();
    }

} // namespace

// =============================================================================
// FUNÇÃO PRINCIPAL DA THREAD
// =============================================================================

void coletor_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer,
                    FaultEventBus &event_bus)
{
    std::cout << "[Coletor " << id << "] is starting." << std::endl;

    // -------------------------------------------------------------------------
    // 1) Abre arquivos CSV
    // -------------------------------------------------------------------------
    std::ofstream csv_file;
    if (!open_csv_log(csv_file))
    {
        std::cerr << "[Coletor " << id << "] Falha ao preparar o arquivo CSV de estados."
                  << " Continuando apenas com logs em stdout." << std::endl;
    }

    std::ofstream events_file;
    if (!open_events_log(events_file))
    {
        std::cerr << "[Coletor " << id << "] Falha ao preparar o arquivo CSV de eventos."
                  << " Eventos não serão persistidos em arquivo." << std::endl;
    }

    try
    {
        // Loop até o 'running_flag' ser falso
        while (running_flag.load())
        {
            // =================================================================
            // 2) LEITURA DE ESTADOS DO BUFFER (BLOQUEANTE)
            // =================================================================
            BufferData data = buffer.read(static_cast<int>(id), running_flag);

            if (!running_flag.load())
            {
                break;
            }

            // Salva no CSV principal (estados)
            if (csv_file.is_open())
            {
                write_csv_line(csv_file, data);
            }

            // Log em stdout (opcional, pode reduzir verbosidade removendo)
            std::cout << "[Coletor " << id << "] "
                      << "ts=" << data.timestamp
                      << ", truck_id=" << data.truck_id
                      << ", x=" << data.i_posicao_x
                      << ", y=" << data.i_posicao_y
                      << ", ang=" << data.i_angulo_x
                      << ", T=" << data.i_temperatura
                      << ", fe=" << std::boolalpha << data.i_falha_eletrica
                      << ", fh=" << std::boolalpha << data.i_falha_hidraulica
                      << std::endl;

            // =================================================================
            // 3) LEITURA NÃO-BLOQUEANTE DE EVENTOS DE FALHA
            // =================================================================
            if (events_file.is_open())
            {
                FaultEvent ev;
                while (event_bus.try_pop(FaultEventBus::Consumer::Coletor, ev))
                {
                    write_event_csv_line(events_file, ev);

                    // Log do evento (opcional com emoji para destaque visual)
                    std::cout << "[Coletor " << id << "] EVENTO: "
                              << fault_event_type_to_string(ev.type)
                              << " - " << ev.description << std::endl;
                }
            }

            // =================================================================
            // 4) Dorme o período configurado (200ms por padrão)
            // =================================================================
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Coletor " << id << "] was interrupted." << std::endl;
    }

    // -------------------------------------------------------------------------
    // 5) Fecha arquivos ao sair
    // -------------------------------------------------------------------------
    if (csv_file.is_open())
    {
        csv_file.close();
    }
    if (events_file.is_open())
    {
        events_file.close();
    }
    std::cout << "[Coletor " << id << "] is stopping." << std::endl;
}

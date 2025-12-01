/**
 * @file coletor_de_dados.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-11-15
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <boost/thread.hpp>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "coletor_de_dados.h"

namespace
{
    // Caminho padrão do arquivo de log CSV
    const std::string kLogDir = "logs";
    const std::string kLogFile = "logs/truck_data.csv";

    // Converte epoch em segundos (UTC) para string ISO 8601 em Brazil/Sao_Paulo (UTC-3)
    std::string format_timestamp_brt(double ts_seconds)
    {
        // Aplica offset fixo de -3 horas (Brasil sem horário de verão)
        double ts_brt = ts_seconds - 3.0 * 3600.0;

        // Parte inteira em segundos
        std::time_t sec = static_cast<std::time_t>(ts_brt);

        // Parte fracionária para milissegundos
        double frac = ts_brt - static_cast<double>(sec);
        if (frac < 0.0)
        {
            // Ajuste para casos muito raros de arredondamento negativo
            sec -= 1;
            frac += 1.0;
        }
        long millis = static_cast<long>(frac * 1000.0 + 0.5); // arredonda

        std::tm tm_brt{};
#if defined(_WIN32)
        gmtime_s(&tm_brt, &sec);
#else
        gmtime_r(&sec, &tm_brt);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_brt, "%Y-%m-%dT%H:%M:%S");

        // acrescenta ".mmm-03:00"
        oss << "." << std::setw(3) << std::setfill('0') << millis
            << "-03:00";

        return oss.str();
    }

    // Garante que a pasta de logs existe e retorna true se estiver ok.
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

    // Abre o arquivo CSV em modo append e escreve o cabeçalho se estiver vazio.
    bool open_csv_log(std::ofstream &ofs)
    {
        if (!ensure_log_directory())
        {
            return false;
        }

        // Verifica se o arquivo já existe e seu tamanho
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

    // Escreve uma linha CSV correspondente a um BufferData
    void write_csv_line(std::ofstream &ofs, const BufferData &data)
    {
        // Timestamp em UTC (ISO 8601)
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

        ofs.flush(); // opcional, mas garante que a linha vai para o disco
    }

}

void coletor_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer)
{
    std::cout << "[Coletor " << id << "] is starting." << std::endl;

    // -------------------------------------------------------------------------
    // 1) Abre o arquivo CSV de log
    // -------------------------------------------------------------------------
    std::ofstream csv_file;
    if (!open_csv_log(csv_file))
    {
        std::cerr << "[Coletor " << id << "] Falha ao preparar o arquivo CSV."
                  << " Continuando apenas com logs em stdout." << std::endl;
    }

    try
    {
        // Loop até o 'running_flag' ser falso
        while (running_flag.load())
        {
            // -----------------------------------------------------------------
            // 2) Leitura BLOQUEANTE do buffer circular (dados TRATADOS)
            // -----------------------------------------------------------------
            BufferData data = buffer.read(static_cast<size_t>(id), running_flag);
            if (!running_flag.load())
            {
                break;
            }

            // -----------------------------------------------------------------
            // 3) Salva a leitura no arquivo CSV (se aberto)
            // -----------------------------------------------------------------
            if (csv_file.is_open())
            {
                write_csv_line(csv_file, data);
            }

            // (Opcional) Também imprimir na tela para debug
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

            // 4) Dorme o período configurado para o Coletor (economia de CPU)
            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "[Coletor " << id << "] was interrupted." << std::endl;
    }

    if (csv_file.is_open())
    {
        csv_file.close();
    }
    std::cout << "[Coletor " << id << "] is stopping." << std::endl;
}

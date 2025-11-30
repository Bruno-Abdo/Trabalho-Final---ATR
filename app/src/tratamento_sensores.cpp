/**
 * @file tratamento_sensores.cpp
 * @author Bruno Abdo (brunoabdo@ufmg.br)
 * @brief
 * @version 0.1
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <iostream>
#include <boost/thread.hpp>
#include "tratamento_sensores.h"

int indx = 0;
boost :: mutex mut;

float mean[] = {33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,
                  53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,
                  71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,
                  89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,
                  105,106,107,108,109,110,111,112,113,114,115,116,117,
                  118,119,120,121,122,123,124,125,126,127,128,129,130,
                  131,132,133,134,135,136,137,138,139,140,141,142, 143,
                  144,145,146,147,148,149,150,151,152,153,154,155,156,
                  157,158,159,160,161,162,163,164,165,166,167,168,169,170,
                  171,172,173,174,175,176,177,178,179,180,181,182,183,184,
                  185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,
                  217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,
                  233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,
                  251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,
                  269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,
                  287,288,289,290,291,292,293,294,295};

float getNext(){
    static const int N = sizeof(mean) / sizeof(mean[0]);

    if (indx >= N) {
        // ou volta pro começo, ou trava, ou repete último valor...
        indx = 0;   // exemplo: torna a sequência circular
    }

    float value = mean[indx];
    std::cout << "indx " << indx << std::endl;
    indx++;
    return value;
}

namespace {

    // Thread para média do ângulo
    void men_angle_thread(int id,
                        int sleep_ms,
                        std::atomic<bool> &running_flag,
                        SharedCircularBuffer &buffer, int M)
    {
       try
        {
            float angle_media = 0.0;
            float angle = 0.0;
            bool initialized = false;
            float buffer_angle[M] = {0};
            int init  = 0;
            int head = 0;
            while (running_flag){   
                //Inicia lendo os M primeiros valores e salva no buffer e calcula a media inicial
                if(init < M){
                    mut.lock();
                    buffer_angle[head] = getNext();
                    mut.unlock();
                    std::cout << "Leitura de novo valor de angle: "<< buffer_angle[head] << std::endl;
                    std::cout << "head: "<<head << std::endl;
                    angle += buffer_angle[head];
                    head = (head + 1) % M;
                    angle_media = angle / ( init + 1 );
                    init++;
                }
                else{
                    angle = angle - buffer_angle[head];
                    //adiciona o novo valor no final do buffer, atualiza a soma e calcula a nova media
                    mut.lock();
                    buffer_angle[head] = getNext();
                    mut.unlock();
                    std::cout << "Leitura de novo valor de angle: "<< buffer_angle[head] << std::endl;
                    std::cout << "head: "<<head << std::endl;
                    angle = angle + buffer_angle[head];
                    head = (head + 1) % M;
                    std::cout << "head: "<<head << std::endl;
                    angle_media = angle / M;
                }

                std::cout << "angle_media: " << angle_media << std::endl;
                boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
            }
        }
        catch (const boost::thread_interrupted &)
        {
            std::cout << "[men_angle] Thread " << id << " foi interrompida." << std::endl;
        }

        std::cout << "[men_angle] Thread " << id << " terminou." << std::endl;
    }

    // Thread para média de X
    void mean_x_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer, int M)
    {
        try
        {
            float x_pos_media = 0.0;
            float soma_x = 0.0;
            bool initialized = false;
            float buffer_x[M] = {0};
            int init  = 0;
            int head = 0;
            while (running_flag){   
                //Inicia lendo os M primeiros valores e salva no buffer e calcula a media inicial
                if(init < M){
                    mut.lock();
                    buffer_x[head] = getNext();
                    mut.unlock();
                    std::cout << "Leitura de novo valor de x_posicao: "<< buffer_x[head] << std::endl;
                    std::cout << "head: "<<head << std::endl;
                    soma_x += buffer_x[head];
                    head = (head + 1) % M;
                    x_pos_media = soma_x / ( init + 1 );
                    init++;
                }
                else{
                    soma_x = soma_x - buffer_x[head];
                    //adiciona o novo valor no final do buffer, atualiza a soma e calcula a nova media
                    mut.lock();
                    buffer_x[head] = getNext();
                    mut.unlock();
                    std::cout << "Leitura de novo valor de x_posicao: "<< buffer_x[head] << std::endl;
                    std::cout << "head: "<<head << std::endl;
                    soma_x = soma_x + buffer_x[head];
                    head = (head + 1) % M;
                    std::cout << "head: "<<head << std::endl;
                    x_pos_media = soma_x / M;
                }

                        std::cout << "x_pos_media: " << x_pos_media << std::endl;
                        boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
            }
        }
        catch (const boost::thread_interrupted &){
            std::cout << "[mean_x] Thread " << id << " foi interrompida." << std::endl;
        }
            std::cout << "[mean_x] Thread " << id << " terminou." << std::endl;
    }

    // Thread para média de Y
    void mean_y_thread(int id,
                    int sleep_ms,
                    std::atomic<bool> &running_flag,
                    SharedCircularBuffer &buffer, int M)
    {
        try
        {
            float y_pos_media = 0.0;
            float soma_y = 0.0;
            bool initialized = false;
            float buffer_y[M] = {0};
            int init  = 0;
            int head = 0;
            while (running_flag){   
                //Inicia lendo os M primeiros valores e salva no buffer e calcula a media inicial
                if(init < M){
                    mut.lock();
                    buffer_y[head] = getNext();
                    mut.unlock();
                    std::cout << "Leitura de novo valor de y_posicao: "<< buffer_y[head] << std::endl;
                    std::cout << "head: "<<head << std::endl;
                    soma_y += buffer_y[head];
                    head = (head + 1) % M;
                    y_pos_media = soma_y / ( init + 1 );
                    init++;
                }
                else{
                    soma_y = soma_y - buffer_y[head];
                    //adiciona o novo valor no final do buffer, atualiza a soma e calcula a nova media
                    mut.lock();
                    buffer_y[head] = getNext();
                    mut.unlock();
                    std::cout << "Leitura de novo valor de y_posicao: "<< buffer_y[head] << std::endl;
                    std::cout << "head: "<<head << std::endl;
                    soma_y = soma_y + buffer_y[head];
                    head = (head + 1) % M;
                    std::cout << "head: "<<head << std::endl;
                    y_pos_media = soma_y / M;
                }

                        std::cout << "y_pos_media: " << y_pos_media << std::endl;
                        boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
            }
        }
        catch (const boost::thread_interrupted &){
            std::cout << "[mean_y] Thread " << id << " foi interrompida." << std::endl;
        }
            std::cout << "[mean_y] Thread " << id << " terminou." << std::endl;
    }

    // Thread para média de temperatura
    void mean_temp_thread(int id,
                        int sleep_ms,
                        std::atomic<bool> &running_flag,
                        SharedCircularBuffer &buffer, int M)
    {
        try
        {
            float temperatura_media = 0.0;
            float soma_temp = 0.0;
            bool initialized = false;
            float buffer_temp[M] = {0};
            int init  = 0;
            int head = 0;

            while (running_flag)
                {   
                    //Inicia lendo os M primeiros valores e salva no buffer e calcula a media inicial
                    if(init < M){
                        mut.lock();
                        buffer_temp[head] = getNext();
                        mut.unlock();
                        std::cout << "Leitura de novo valor de temperatura: "<< buffer_temp[head] << std::endl;
                        std::cout << "head: "<<head << std::endl;
                        soma_temp += buffer_temp[head];
                        head = (head + 1) % M;
                        temperatura_media = soma_temp / ( init + 1 );
                        init++;
                    }
                    else{
                        soma_temp = soma_temp - buffer_temp[head];
                        //adiciona o novo valor no final do buffer, atualiza a soma e calcula a nova media
                        mut.lock();
                        buffer_temp[head] = getNext();
                        mut.unlock();
                        std::cout << "Leitura de novo valor de temperatura: "<< buffer_temp[head] << std::endl;
                        std::cout << "head: "<<head << std::endl;
                        soma_temp = soma_temp + buffer_temp[head];
                        head = (head + 1) % M;
                        std::cout << "head: "<<head << std::endl;
                        temperatura_media = soma_temp / M;
                    }

                    std::cout << "Temperatura media: " << temperatura_media << std::endl;
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
                }
        }
        catch (const boost::thread_interrupted &)
        {
            std::cout << "Temp Thread " << id << " foi interrompida." << std::endl;
        }

        std::cout << "Temp Thread " << id << " terminou." << std::endl;
    }
} // namespace

void tratamento_thread(int id,
                       int sleep_ms,
                       std::atomic<bool> &running_flag,
                       SharedCircularBuffer &buffer)
{
    std::cout << "Tratamento " << id << " is starting." << std::endl;

    boost::thread t_men_angle(men_angle_thread,
                              id,
                              sleep_ms,
                              std::ref(running_flag),
                              std::ref(buffer), 10);

    boost::thread t_mean_x(mean_x_thread,
                           id,
                           sleep_ms,
                           std::ref(running_flag),
                           std::ref(buffer),10);

    boost::thread t_mean_y(mean_y_thread,
                           id,
                           sleep_ms,
                           std::ref(running_flag),
                           std::ref(buffer),10);

    boost::thread t_mean_temp(mean_temp_thread,
                              id,
                              sleep_ms,
                              std::ref(running_flag),
                              std::ref(buffer),10);

    try{
        while (running_flag)
        {
            std::cout << "[LOG] Tratamento " << id << " is running..." << std::endl;

            BufferData data;
            // ... lê sensores físicos, filtra, preenche 'data' ...
            data.i_posicao_x = 10;
            data.i_temperatura = 45;

            buffer.write(data, running_flag);

            boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_ms));
        }
    }
    catch (const boost::thread_interrupted &)
    {
        std::cout << "Tratamento " << id << " was interrupted." << std::endl;
    }

    // 3) Antes de sair, garante que as threads filhas parem e sejam juntadas
    running_flag = false; // garante que os loops delas também terminem

    t_men_angle.interrupt();
    t_mean_x.interrupt();
    t_mean_y.interrupt();
    t_mean_temp.interrupt();

    t_men_angle.join();
    t_mean_x.join();
    t_mean_y.join();
    t_mean_temp.join();

    std::cout << "Tratamento " << id << " is stopping." << std::endl;
}
 
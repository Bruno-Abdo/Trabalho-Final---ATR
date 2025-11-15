# --------------------------- EXECUTABLE CONFIGURATION --------------------------- #

# Set the name of the final executable target
set(ATR_EXEC_NAME atr)

# Define the main executable and specify its source files
add_executable(${ATR_EXEC_NAME}
    main.cpp
    src/coletor_de_dados.cpp
    src/controle_de_navegacao.cpp
    src/logica_de_comando.cpp
    src/monitoramento_de_falhas.cpp
    src/planejamento_de_rota.cpp
    src/tratamento_sensores.cpp
)

# Add include directories needed for this target (e.g., generated config headers)
target_include_directories(${ATR_EXEC_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# Link required internal libraries to the executable
target_link_libraries(${ATR_EXEC_NAME} PRIVATE
    nlohmann_json::nlohmann_json       
    PahoMqttCpp::paho-mqttpp3 
    Boost::system
    Boost::thread
    Boost::chrono
    Threads::Threads
)

# Define any required compile-time macros for the target
target_compile_definitions(${ATR_EXEC_NAME} PRIVATE
)

# --------------------------- OUTPUT DIRECTORY --------------------------- #

# Place the built executable in a structured 'project_output' directory
# within the build tree for easier discovery and organization
set_target_properties(${ATR_EXEC_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/project_output
)

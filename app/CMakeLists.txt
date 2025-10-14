# --------------------------- EXECUTABLE CONFIGURATION --------------------------- #

# Set the name of the final executable target
set(ATR_EXEC_NAME atr)

# Define the main executable and specify its source files
add_executable(${ATR_EXEC_NAME}
    main.cpp
)

# Add include directories needed for this target (e.g., generated config headers)
target_include_directories(${ATR_EXEC_NAME} PRIVATE

)

# Link required internal libraries to the executable
target_link_libraries(${ATR_EXEC_NAME} PRIVATE
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

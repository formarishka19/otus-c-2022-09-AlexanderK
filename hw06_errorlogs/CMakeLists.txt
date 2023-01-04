cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(app)
set(BINARY_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/bin")
add_subdirectory(static)

add_executable(app main.c)
set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Wpedantic -std=c11")
target_link_libraries(app alog)
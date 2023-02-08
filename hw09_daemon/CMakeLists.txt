cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(daemon)
set(BINARY_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/../")
add_subdirectory(lib/libconfig)

add_executable(daemon main.c)
set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS "-D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -std=c11")
target_link_libraries(daemon libconfig)
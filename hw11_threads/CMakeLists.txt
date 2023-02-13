cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(parse)
set(BINARY_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/../")


find_package(PkgConfig REQUIRED) 
pkg_search_module(GLIB REQUIRED glib-2.0) 


add_executable(${PROJECT_NAME} main.c)

set_source_files_properties(main.c COMPILE_FLAGS "-Wall -Wextra -Wpedantic -std=c11 ")
target_include_directories(${PROJECT_NAME} PUBLIC ${GLIB_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC ${GLIB_LDFLAGS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${GLIB_LIBRARIES})
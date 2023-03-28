
cmake_minimum_required(VERSION 3.5.2)

project(logsend C)

set(BINARY_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/bin")
add_subdirectory(wsServer)
add_subdirectory(src)

option(VALIDATE_UTF8 "Enable UTF-8 validation (default ON)" ON)
if(VALIDATE_UTF8)
	target_compile_definitions(logsend PRIVATE VALIDATE_UTF8)
endif(VALIDATE_UTF8)


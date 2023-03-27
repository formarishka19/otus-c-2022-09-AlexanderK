add_executable(logsend logsend.c)
set_source_files_properties(logsend.c PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Wpedantic -std=c11")
target_link_libraries(logsend ws)

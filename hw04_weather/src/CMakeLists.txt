find_package(CURL REQUIRED)
if(CURL_FOUND)
    message(STATUS "Found CURL version: ${CURL_VERSION_STRING}")
    message(STATUS "Using CURL include dir(s): ${CURL_INCLUDE_DIRS}")
    message(STATUS "Using CURL lib(s): ${CURL_LIBRARIES}")
    include_directories(${CURL_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Could not find CURL")
endif()

add_executable(weather main.c)
set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Wpedantic -std=c11")
target_link_libraries(weather cjson ${CURL_LIBRARIES})
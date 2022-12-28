cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(player)
set(BINARY_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/build")

# find_package(GLIB REQUIRED glib-2.0)
# if(GLIB_FOUND)
#     message(STATUS "Found CURL version: ${GLIB_VERSION_STRING}")
#     message(STATUS "Using CURL include dir(s): ${GLIB_INCLUDE_DIRS}")
#     message(STATUS "Using CURL lib(s): ${GLIB_LIBRARIES}")
#     include_directories(${GLIB_INCLUDE_DIRS})
# else()
#     message(FATAL_ERROR "Could not find GLIB")
# endif()

# find_package(GStreamer REQUIRED)
# if(GStreamer_FOUND)
#     message(STATUS "Found CURL version: ${GStreamer_VERSION_STRING}")
#     message(STATUS "Using CURL include dir(s): ${GStreamer_INCLUDE_DIRS}")
#     message(STATUS "Using CURL lib(s): ${GStreamer_LIBRARIES}")
#     include_directories(${GStreamer_INCLUDE_DIRS})
# else()
#     message(FATAL_ERROR "Could not find GStreamer")
# endif()

find_package(PkgConfig) 
pkg_search_module(GLIB REQUIRED glib-2.0) 
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-base-1.0 gstreamer-audio-1.0)

add_executable(${PROJECT_NAME} main.c)
set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Wpedantic -std=c11")
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    )
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    ${GSTREAMER_LINK_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GLIB_LINK_LIBRARIES}
    )
# target_include_directories(${PROJECT_NAME} PRIVATE ${GSTREAMER_BASE_INCLUDE_DIRS})
# target_link_libraries(${PROJECT_NAME} gst-plugins-good)
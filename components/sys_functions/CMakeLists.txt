cmake_minimum_required(VERSION 3.16.0)

FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if(DEFINED ESP_PLATFORM AND ESP_PLATFORM)
    # ESP-IDF settings
    idf_component_register(
        SRCS ${app_sources}
        INCLUDE_DIRS "include"
    )
else()
    # Standard CMake settings
    add_library(sys_functions STATIC
        ${app_sources}
    )
    target_include_directories(sys_functions PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

project(sys_functions)

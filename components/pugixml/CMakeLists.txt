cmake_minimum_required(VERSION 3.16.0)

if(DEFINED ESP_PLATFORM AND ESP_PLATFORM)
    # ESP-IDF specific settings
    idf_component_register(
        SRCS "src/pugixml.cpp"
        INCLUDE_DIRS "src"
    )
else()
    # Standard CMake settings
    add_library(pugixml STATIC
        src/pugixml.cpp
    )
    target_include_directories(pugixml PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
endif()

project(pugixml)

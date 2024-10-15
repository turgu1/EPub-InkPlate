
cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)
list(FILTER app_sources EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/src/embed/*.*")

set(components
    global
    miniz
    pugixml
    sys_functions
    freertos
    ESP-IDF-InkPlate
    inkplate_screen
    memory_pool
    pictures
    esp_http_server
    esp_app_format
    pthread
    freetype
)

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." REQUIRES ${components})

target_add_binary_data(${COMPONENT_TARGET} "embed/favicon.ico" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/upload_script.html" TEXT)


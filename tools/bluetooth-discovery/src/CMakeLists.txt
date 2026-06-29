
cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)
list(FILTER app_sources EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/src/embed/*.*")

# Include the ESP32 regression-loop test when REGRESSION_TEST is enabled
if(DEFINED REGRESSION_TEST AND REGRESSION_TEST)
  list(APPEND app_sources ${CMAKE_SOURCE_DIR}/test/esp32_regression_loop.cpp)
  add_compile_definitions(REGRESSION_TEST=1)
  message(STATUS "Regression loop test ENABLED")
endif()

# Even if we don't use all of them, easier to get them all and let the linker do it's job...

set(components
    global
    config
    zip
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
    spi_flash
    himem
    simple_db
    display_list
    simple_list
    bt
)

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." REQUIRES ${components})

target_add_binary_data(${COMPONENT_TARGET} "embed/RobotoCondensed-Regular.otf" BINARY)

cmake_minimum_required(VERSION 3.16)

# set(DEVICE "INKPLATE_6_V2")
set(DEVICE "INKPLATE_10_V2")

set(APP_VERSION "1.0.0")

set(IDF_TARGET esp32)

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wno-ignored-qualifiers")

add_compile_definitions(
    EPUB_LOG_LEVEL=ESP_LOG_DEBUG
    DATE_TIME_RTC=1
    PNGLE_GRAYSCALE_OUTPUT=1
    PNGLE_NO_GAMMA_CORRECTION=1
)

if(DEVICE MATCHES "^INKPLATE_6_V2$")
    message("INKPLATE 6 V2 defined")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.ble")
    set(DEVICE_NAME "I6V2")
    add_compile_definitions(
        INKPLATE_6_V2=1
        PCAL6416=1)
elseif(DEVICE MATCHES "^INKPLATE_10_V2$")
    message("INKPLATE 10 V2 defined")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.ble")
    set(DEVICE_NAME "I10V2")
    add_compile_definitions(
        INKPLATE_10_V2=1
        DMA_ENABLE=0
        PCAL6416=1)

else()
    message(FATAL_ERROR "DEVICE NOT defined!!\nExample call: idf.py build -DDEVICE=INKPLATE_6_V2 -DAPP_VERSION=1.0.0")
endif()

list(APPEND EXTRA_COMPONENT_DIRS 
    src 
    ../../components
)

string(TIMESTAMP THE_TIME "%Y%m%d-%H%MZ" UTC)

set(PROJECT_VER "${APP_VERSION}-${DEVICE_NAME}-${THE_TIME}") 

add_compile_definitions(
    APP_VERSION="${APP_VERSION}"
    EPUB_INKPLATE_BUILD=1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Bluetooth-Discovery)

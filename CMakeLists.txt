cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED DEVICE)
# set(DEVICE "INKPLATE_5_V2")
# set(DEVICE "INKPLATE_6")
# set(DEVICE "INKPLATE_6_EXTENDED")
# set(DEVICE "INKPLATE_6_BLUETOOTH")
# set(DEVICE "INKPLATE_6_V2")
# set(DEVICE "INKPLATE_10")
# set(DEVICE "INKPLATE_10_EXTENDED")
set(DEVICE "INKPLATE_10_V2")
# set(DEVICE "INKPLATE_6FLICK")
# set(DEVICE "INKPLATE_6PLUS")
# set(DEVICE "INKPLATE_6PLUS_V2")

# set(DEBUGGING 1)
# set(REGRESSION_TEST 1)
endif()

set(APP_VERSION "3.0.0")

set(IDF_TARGET esp32)

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wno-ignored-qualifiers")

add_compile_definitions(
    EPUB_LOG_LEVEL=ESP_LOG_DEBUG
    DATE_TIME_RTC=1
    PNGLE_GRAYSCALE_OUTPUT=1
    PNGLE_NO_GAMMA_CORRECTION=1
)

if(DEVICE MATCHES "^INKPLATE_5_V2$")
    message("INKPLATE 5 V2 defined")
    set(DEVICE_NAME "I5V2B")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.ble")
    add_compile_definitions(
        INKPLATE_5_V2=1 
        PCAL6416=1
        BLE_KEYPAD=1)
elseif(DEVICE MATCHES "^INKPLATE_6$")
    message("INKPLATE 6 defined")
    set(DEVICE_NAME "I6")
    add_compile_definitions(
        INKPLATE_6=1 
        MCP23017=1)
elseif(DEVICE MATCHES "^INKPLATE_6_EXTENDED$")
    message("INKPLATE 6 EXTENDED defined")
    set(DEVICE_NAME "I6E")
    add_compile_definitions(
        INKPLATE_6=1
        EXTENDED_CASE=1
        MCP23017=1)
elseif(DEVICE MATCHES  "^INKPLATE_6PLUS$")
    message("INKPLATE 6PLUS defined")
    set(DEVICE_NAME "I6P")
    add_compile_definitions(
        INKPLATE_6PLUS=1 
        MCP23017=1)
elseif(DEVICE MATCHES "^INKPLATE_6PLUS_V2$")
    message("INKPLATE 6PLUS V2 defined")
    set(DEVICE_NAME "I6PV2")
    add_compile_definitions(
        INKPLATE_6PLUS_V2=1 
        PCAL6416=1)
elseif(DEVICE MATCHES "^INKPLATE_6FLICK$")
    message("INKPLATE 6FLICK defined")
    set(DEVICE_NAME "I6F")
    add_compile_definitions(
        INKPLATE_6FLICK=1 
        PCAL6416=1)
elseif(DEVICE MATCHES "^INKPLATE_10$")
    message("INKPLATE 10 defined")
    set(DEVICE_NAME "I10")
    add_compile_definitions(
        INKPLATE_10=1
        MCP23017=1)
elseif(DEVICE MATCHES "^INKPLATE_10_EXTENDED$")
    message("INKPLATE 10 EXTENDED defined")
    set(DEVICE_NAME "I10E")
    add_compile_definitions(
        INKPLATE_10=1
        EXTENDED_CASE=1
        MCP23017=1)
elseif(DEVICE MATCHES "^INKPLATE_6_V2$")
    message("INKPLATE 6 V2 defined")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.ble")
    set(DEVICE_NAME "I6V2")
    add_compile_definitions(
        INKPLATE_6_V2=1
        BLE_KEYPAD=1
        PCAL6416=1)
elseif(DEVICE MATCHES "^INKPLATE_10_V2$")
    message("INKPLATE 10 V2 defined")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.ble")
    set(DEVICE_NAME "I10V2")
    add_compile_definitions(
        INKPLATE_10_V2=1
        DMA_ENABLE=0
        BLE_KEYPAD=1
        PCAL6416=1)

else()
    message(FATAL_ERROR "DEVICE NOT defined!!\nExample call: idf.py build -DDEVICE=INKPLATE_6 -DAPP_VERSION=3.0.0")
endif()

list(APPEND EXTRA_COMPONENT_DIRS 
    src 
    # ../ESP-IDF-InkPlate
)

string(TIMESTAMP THE_TIME "%Y%m%d-%H%MZ" UTC)

set(PROJECT_VER "${APP_VERSION}-${DEVICE_NAME}-${THE_TIME}") 

add_compile_definitions(
    APP_VERSION="${APP_VERSION}"
    EPUB_INKPLATE_BUILD=1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(EPub-InkPlate)

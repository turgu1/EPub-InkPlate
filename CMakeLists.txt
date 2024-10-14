cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32)

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

add_compile_definitions(
    EPUB_LOG_LEVEL=ESP_LOG_DEBUG
    DATE_TIME_RTC=1
    PNGLE_GRAYSCALE_OUTPUT=1
    PNGLE_NO_GAMMA_CORRECTION=1
)

if(DEVICE MATCHES "^INKPLATE_6$")
    message("INKPLATE 6 defined")
    set(DEVICE_NAME "I6")
    add_compile_definitions(
        EPUB_INKPLATE_BUILD=1 
        INKPLATE_6=1 
        MCP23017=1)
elseif(DEVICE MATCHES  "^INKPLATE_6PLUS$")
    message("INKPLATE 6PLUS defined")
    set(DEVICE_NAME "I6P")
    add_compile_definitions(
        EPUB_INKPLATE_BUILD=1 
        INKPLATE_6PLUS=1 
        MCP23017=1)
elseif(DEVICE MATCHES "^INKPLATE_6PLUS_V2$")
    message("INKPLATE 6PLUS V2 defined")
    set(DEVICE_NAME "I6PV2")
    add_compile_definitions(
        EPUB_INKPLATE_BUILD=1 
        INKPLATE_6PLUS_V2=1 
        PCAL6416=1)
elseif(DEVICE MATCHES "^INKPLATE_6FLICK$")
    message("INKPLATE 6FLICK defined")
    set(DEVICE_NAME "I6F")
    add_compile_definitions(
        EPUB_INKPLATE_BUILD=1 
        INKPLATE_6FLICK=1 
        PCAL6416=1)
elseif(DEVICE MATCHES "^INKPLATE_10$")
    message("INKPLATE 10 defined")
    set(DEVICE_NAME "I10")
    add_compile_definitions(
        EPUB_INKPLATE_BUILD=1 
        INKPLATE_10=1 
        MCP23017=1)
elseif(DEVICE MATCHES "^INKPLATE_10_V2$")
    message("INKPLATE 10 V2 defined")
    set(DEVICE_NAME "I10V2")
    add_compile_definitions(
        EPUB_INKPLATE_BUILD=1 
        INKPLATE_10_V2=1 
        PCAL6416=1)
else()
    message(FATAL_ERROR "DEVICE NOT defined!!\nExample call: idf.py build -DDEVICE=INKPLATE_6 -DAPP_VERSION=V3.0.1")
endif()

list(APPEND EXTRA_COMPONENT_DIRS 
    "/home/turgu1/Dev/ESP-IDF-InkPlate"
    src 
)

string(TIMESTAMP THE_TIME "%Y/%m/%d-%H:%M:%SZ" UTC)

set(PROJECT_VER "${APP_VERSION}-${DEVICE_NAME}-${THE_TIME}") 

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(EPub-InkPlate)

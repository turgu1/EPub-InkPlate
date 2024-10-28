cmake_minimum_required(VERSION 3.5)

idf_component_register(INCLUDE_DIRS "include/freetype2")

add_prebuilt_library(libfreetype "lib/libfreetype.a"
             PRIV_REQUIRES spi_flash app_update log)

target_link_libraries(${COMPONENT_LIB} INTERFACE libfreetype)

project(freetype)
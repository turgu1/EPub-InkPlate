FILE(GLOB_RECURSE screen_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components global himem pictures zip)

idf_component_register(SRCS ${screen_sources} INCLUDE_DIRS "src" REQUIRES ${components})

project(image)
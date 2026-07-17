FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components global frozen)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

project(utf8)
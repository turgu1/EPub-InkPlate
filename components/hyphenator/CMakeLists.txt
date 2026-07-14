FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components global himem)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

target_add_binary_data(${COMPONENT_TARGET} "embed/en.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/fr.bin" BINARY)

project(hyphenator)
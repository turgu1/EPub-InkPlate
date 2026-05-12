FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

set(components global config himem freetype memory_pool inkplate_screen)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

project(fonts)

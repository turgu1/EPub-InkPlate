FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

set(components global memory_pool pictures)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

project(display_list)

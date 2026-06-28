
FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components global himem memory_pool)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})


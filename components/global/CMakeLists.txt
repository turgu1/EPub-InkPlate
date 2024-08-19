FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components ESP-IDF-InkPlate sys_functions)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})
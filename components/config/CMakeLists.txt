FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

set(components global ESP-IDF-EInk-Platform himem pugixml)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

project(config)

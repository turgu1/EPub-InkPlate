FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src")
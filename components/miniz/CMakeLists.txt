
FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/miniz/*.*)

idf_component_register(SRCS ${sources} INCLUDE_DIRS ".")

project(miniz)

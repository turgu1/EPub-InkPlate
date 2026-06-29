FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components 
  global
  freertos
  bt
)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

project(ble_keypad)
FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

idf_component_register(
  SRCS         ${sources}
  INCLUDE_DIRS "src"
  REQUIRES     esp_hw_support   # provides esp_memory_utils.h for esp_ptr_external_ram()
            simple_list
)

project(himem)
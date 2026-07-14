FILE(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(components global himem)

idf_component_register(SRCS ${sources} INCLUDE_DIRS "src" REQUIRES ${components})

target_add_binary_data(${COMPONENT_TARGET} "embed/en.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/fr.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/cs.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/da.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/de.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/es.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/hr.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/is.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/it.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/nl.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/pl.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/pt.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/sl.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/sk.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/sq.bin" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "embed/tk.bin" BINARY)

project(hyphenator)
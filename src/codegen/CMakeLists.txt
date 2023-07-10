file(GLOB CODEGEN_SRC_FILES *.cc *.hh)

add_library(codegen ${CODEGEN_SRC_FILES})

target_include_directories(
    codegen
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    codegen
    PRIVATE ir
    PRIVATE mir
    PRIVATE mir_builder
    PRIVATE utils
)

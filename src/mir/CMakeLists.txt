file(GLOB CODEGEN_SRC_FILES *.cc *.hh)

add_library(mir ${CODEGEN_SRC_FILES})

target_include_directories(
    mir
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/src/codegen
)

target_link_libraries(
    mir
    PRIVATE ir
    PRIVATE utils
)

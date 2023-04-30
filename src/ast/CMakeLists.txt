add_subdirectory(grammer)

add_library(
    ast
    ast.hh
    ast.cc
    raw_ast.hh
    raw_ast.cc
    ast_printer.hh
    ast_printer.cc
)

target_include_directories(
    ast
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ast
    PRIVATE parser
    PRIVATE utils
)

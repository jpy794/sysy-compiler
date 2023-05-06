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

# FIXME: https://github.com/antlr/antlr4/issues/4254
target_compile_options(
    ast
    PRIVATE -Wno-overloaded-virtual
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

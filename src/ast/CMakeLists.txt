add_subdirectory(grammer)

add_library(
    ast
    ast.hh
    ast.cc
    raw_ast.hh
    raw_ast.cc
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

add_library(
    astPrinter
    astprinter.cc
    astprinter.hh
)

target_include_directories(
    astPrinter
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    astPrinter
    PRIVATE ast
)

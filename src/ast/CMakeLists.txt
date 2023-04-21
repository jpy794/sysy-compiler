add_subdirectory(grammer)

add_library(
    ast
    ast.cc
)

target_include_directories(
    ast
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ast
    PRIVATE parser
)
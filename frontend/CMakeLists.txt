add_subdirectory(grammer)

add_library(
    frontend
    ast.cc
)

target_include_directories(
    frontend
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(frontend PRIVATE parser)

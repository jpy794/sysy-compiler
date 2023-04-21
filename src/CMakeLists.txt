# common headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

add_subdirectory(ast)

add_executable(sysyc main.cc)

target_link_libraries(
    sysyc
    ast
)

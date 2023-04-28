add_subdirectory(ast)
# add_subdirectory(ir)
add_subdirectory(utils)

add_executable(sysyc main.cc)

target_link_libraries(
    sysyc
    ast
    # ir
)

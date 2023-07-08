add_subdirectory(ast)
add_subdirectory(ir)
add_subdirectory(ir_builder)
add_subdirectory(utils)
add_subdirectory(pass)
add_subdirectory(transform)
add_subdirectory(analysis)

add_executable(sysyc main.cc)

target_link_libraries(
    sysyc
    PRIVATE ast
    PRIVATE ir
    PRIVATE ir_builder
    PRIVATE utils
    PRIVATE pass
    PRIVATE transform
    PRIVATE analysis
)

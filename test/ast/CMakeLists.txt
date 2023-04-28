add_executable(ASTPrint ASTPrint.cc)

target_link_libraries(
    ASTPrint
    PRIVATE utils
    PRIVATE ast
    PRIVATE parser
)

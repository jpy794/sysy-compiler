add_executable(test_ast test_ast.cc)

target_link_libraries(
    test_ast
    PRIVATE ast
)

function(ast_test BASE CASE)
    add_test(
        test_ast_${CASE}
        test_ast
        ${CMAKE_SOURCE_DIR}/test/cases/${BASE}/${CASE}.sy
        out/${BASE}/${CASE}.json)
endfunction()

ast_test(2022/func 00_main)

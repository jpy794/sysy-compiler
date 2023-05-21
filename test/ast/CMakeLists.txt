add_executable(test_ast test_ast.cc)

target_link_libraries(
    test_ast
    PRIVATE ast
)

set(TESTCASES_BASE ${CMAKE_SOURCE_DIR}/test/sysy_tests/cases)

function(ast_test BASE CASE)
    add_test(
        test_ast_${CASE}
        test_ast
        ${TESTCASES_BASE}/${BASE}/${CASE}.sy
        out/${BASE}/${CASE}.json
    )
endfunction()

function(ast_test_path BASE)
    file(
        GLOB CASES_SY
        RELATIVE ${TESTCASES_BASE}/${BASE}
        ${TESTCASES_BASE}/${BASE}/*.sy
    )
    foreach(CASE_SY ${CASES_SY})
        get_filename_component(CASE ${CASE_SY} NAME_WE) 
        ast_test(${BASE} ${CASE}) 
    endforeach()
endfunction()

ast_test_path(2022/func)
ast_test_path(ast)

add_custom_target(
    check_ast
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_ast_'
)

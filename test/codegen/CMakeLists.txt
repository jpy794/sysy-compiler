add_executable(simple-test simple_test.cc)

target_link_libraries(
    simple-test
    PRIVATE ast
    PRIVATE ir
    PRIVATE ir_builder
    PRIVATE mir
    PRIVATE mir_builder
    PRIVATE codegen
    PRIVATE pass
    PRIVATE analysis
    PRIVATE transform
    PRIVATE utils
)

add_executable(test_codegen test_codegen.cc)

target_link_libraries(
    test_codegen
    PRIVATE ast
    PRIVATE ir
    PRIVATE ir_builder
    PRIVATE mir
    PRIVATE mir_builder
    PRIVATE codegen
    PRIVATE pass
    PRIVATE analysis
    PRIVATE transform
    PRIVATE utils
)

set(SYSY_TESTS ${CMAKE_SOURCE_DIR}/test/sysy_tests)

function(add_codegen_test BASE CASE)
    add_test(
        test_codegen_${CASE}
        test_codegen
        ${CASE}
        out
        ${SYSY_TESTS}/cases/${BASE}
        ${SYSY_TESTS}/in/${BASE}
        ${SYSY_TESTS}/out/${BASE}
        ${SYSY_TESTS}/lib/sylib.c
    )
    set_tests_properties(test_codegen_${CASE} PROPERTIES TIMEOUT 100) 
endfunction()

function(add_codegen_test_path BASE)
    file(
        GLOB CASES_SY
        RELATIVE ${SYSY_TESTS}/cases/${BASE}
        ${SYSY_TESTS}/cases/${BASE}/*.sy
    )
    foreach(CASE_SY ${CASES_SY})
        get_filename_component(CASE ${CASE_SY} NAME_WE) 
        add_codegen_test(${BASE} ${CASE}) 
    endforeach()
endfunction()

add_codegen_test_path(2022/func)
add_codegen_test_path(2022/func_hidden)
add_codegen_test_path(2022/perf)

add_custom_target(
    check_codegen
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_codegen_'
)

add_executable(test_ir_builder test_ir_builder.cc)

target_link_libraries(
    test_ir_builder
    PRIVATE ast
    PRIVATE ir
    PRIVATE utils
    PRIVATE ir_builder
)

set(SYSY_TESTS ${CMAKE_SOURCE_DIR}/test/sysy_tests)

function(ir_builder_test BASE CASE)
    add_test(
        test_ir_builder_${CASE}
        test_ir_builder
        ${CASE}
        out
        ${SYSY_TESTS}/cases/${BASE}
        ${SYSY_TESTS}/in/${BASE}
        ${SYSY_TESTS}/out/${BASE}
        ${SYSY_TESTS}/lib/sylib.c
    )
    set_tests_properties(test_ir_builder_${CASE} PROPERTIES TIMEOUT 100) 
endfunction()

function(ir_builder_test_path BASE)
    file(
        GLOB CASES_SY
        RELATIVE ${SYSY_TESTS}/cases/${BASE}
        ${SYSY_TESTS}/cases/${BASE}/*.sy
    )
    foreach(CASE_SY ${CASES_SY})
        get_filename_component(CASE ${CASE_SY} NAME_WE) 
        ir_builder_test(${BASE} ${CASE}) 
    endforeach()
endfunction()

ir_builder_test_path(2022/func)
ir_builder_test_path(2022/func_hidden)
ir_builder_test_path(2022/perf)

add_custom_target(
    check_ir_builder
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_ir_builder_'
)

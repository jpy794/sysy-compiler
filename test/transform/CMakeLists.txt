add_executable(
    test_transform
    test_transform.cc
)


target_link_libraries(
    test_transform
    PRIVATE ast
    PRIVATE ir
    PRIVATE ir_builder
    PRIVATE pass
    PRIVATE analysis
    PRIVATE transform
    PRIVATE utils
)

set(SYSY_TESTS ${CMAKE_SOURCE_DIR}/test/sysy_tests)

function(transform_test BASE CASE)
    add_test(
        test_transform_${CASE}
        test_transform
        ${CASE}
        out
        ${SYSY_TESTS}/cases/${BASE}
        ${SYSY_TESTS}/in/${BASE}
        ${SYSY_TESTS}/out/${BASE}
        ${SYSY_TESTS}/lib/sylib.c
    )
    set_tests_properties(test_transform_${CASE} PROPERTIES TIMEOUT 100) 
endfunction()

function(transform_test_path BASE)
    file(
        GLOB CASES_SY
        RELATIVE ${SYSY_TESTS}/cases/${BASE}
        ${SYSY_TESTS}/cases/${BASE}/*.sy
    )
    foreach(CASE_SY ${CASES_SY})
        get_filename_component(CASE ${CASE_SY} NAME_WE) 
        transform_test(${BASE} ${CASE}) 
    endforeach()
endfunction()

transform_test_path(2022/func)
transform_test_path(2022/func_hidden)
transform_test_path(2022/perf)

add_custom_target(
    check_transform
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_transform_'
)

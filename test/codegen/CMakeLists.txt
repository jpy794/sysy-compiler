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

set(SYSY_TESTS ${CMAKE_SOURCE_DIR}/test/cases)

function(add_codegen_test CASE_FULL_PATH)
    get_filename_component(CASE ${CASE_FULL_PATH} NAME_WE)
    add_test(
        test_codegen_${CASE}
        test_codegen
        ${CASE_FULL_PATH}
    )
    set_tests_properties(test_codegen_${CASE} PROPERTIES TIMEOUT 100) 
endfunction()

function(add_codegen_test_path CATEGORY)
    file(
        GLOB CASES_SY
        ${SYSY_TESTS}/${CATEGORY}/*.sy
    )
    foreach(CASE_SY ${CASES_SY})
        add_codegen_test(${CASE_SY}) 
    endforeach()
endfunction()

add_codegen_test_path(func)
add_codegen_test_path(func_hidden)
add_codegen_test_path(perf)

add_custom_target(
    check_codegen
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_codegen_'
)

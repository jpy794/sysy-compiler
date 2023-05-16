add_executable(test_ir_builder test_ir_builder.cc)

target_link_libraries(
    test_ir_builder
    PRIVATE ast
    PRIVATE ir
    PRIVATE utils
    PRIVATE ir_builder
)

set(TESTCASES_BASE ${CMAKE_SOURCE_DIR}/test/cases)

function(ir_builder_test BASE CASE)
    add_test(
        test_ir_builder_${CASE}
        test_ir_builder
        ${CASE}
        out
        ${TESTCASES_BASE}/${BASE}
        ${TESTCASES_BASE}/${BASE}/in
        ${TESTCASES_BASE}/${BASE}/out
    )
endfunction()

function(ir_builder_test_path BASE)
    file(
        GLOB CASES_SY
        RELATIVE ${TESTCASES_BASE}/${BASE}
        ${TESTCASES_BASE}/${BASE}/*.sy
    )
    foreach(CASE_SY ${CASES_SY})
        get_filename_component(CASE ${CASE_SY} NAME_WE) 
        ir_builder_test(${BASE} ${CASE}) 
    endforeach()
endfunction()

ir_builder_test_path(2022/func)

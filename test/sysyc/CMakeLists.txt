
function(sysyc_test PREFIX CASE SYSYC_ARGS)
    get_filename_component(CASE_NAME ${CASE} NAME_WE)
    add_test(
        NAME test_sysyc_${PREFIX}_${CASE_NAME}
        COMMAND ${CMAKE_SOURCE_DIR}/tool/sysyc_test.sh ${CASE} ${CMAKE_CURRENT_BINARY_DIR}/${PREFIX} ${SYSYC_ARGS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(test_sysyc_${PREFIX}_${CASE_NAME} PROPERTIES TIMEOUT 500) 
endfunction()

function(sysyc_test_path BASE)
    file(
        GLOB CASES
        ${CMAKE_SOURCE_DIR}/test/cases/${BASE}/*.sy
    )
    foreach(CASE ${CASES})
        sysyc_test("ir_${BASE}" ${CASE} "-emit-llvm")
        sysyc_test("ir_opt_${BASE}" ${CASE} "-emit-llvm -O1")
        sysyc_test("asm_${BASE}" ${CASE} "")
        sysyc_test("asm_opt_${BASE}" ${CASE} "-O1")
    endforeach()
endfunction()

sysyc_test_path(simplyfied_test)
sysyc_test_path(func)
sysyc_test_path(func_hidden)
sysyc_test_path(perf)

add_custom_target(
    check_sysyc_ir
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_sysyc_ir_.*' -E 'test_sysyc_ir_opt_.*'
)

add_custom_target(
    check_sysyc_ir_opt
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_sysyc_ir_opt_.*'
)

add_custom_target(
    check_sysyc_ir_opt_perf
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_sysyc_ir_opt_perf_.*'
)

add_custom_target(
    check_sysyc_asm
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_sysyc_asm_.*' -E 'test_sysyc_asm_opt_.*'
)

add_custom_target(
    check_sysyc_asm_opt
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_sysyc_asm_opt_.*'
)

add_custom_target(
    check_sysyc_asm_opt_perf
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R 'test_sysyc_asm_opt_perf_.*'
)

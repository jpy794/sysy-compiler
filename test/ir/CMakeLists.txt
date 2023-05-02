add_executable(test_ir_gcd test_ir_gcd.cc)

target_link_libraries(
    test_ir_gcd
    PRIVATE utils
    PRIVATE ir
)

add_test(
    NAME test-ir-naive-version
    COMMAND test_ir_gcd
)

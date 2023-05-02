add_executable(test_ir_gcd test_ir_gcd.cc)

target_link_libraries(
    test_ir_gcd
    PUBLIC ir
    PRIVATE utils
)

add_test(
    NAME test-ir-naive-version
    COMMAND test_ir_gcd
)

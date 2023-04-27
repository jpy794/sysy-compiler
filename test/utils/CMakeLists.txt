add_executable(test_ilist test_ilist.cc)

target_include_directories(
    test_ilist
    PRIVATE ${CMAKE_SOURCE_DIR}/src/utils
)

add_test(
    NAME test_ilist
    COMMAND test_ilist
)
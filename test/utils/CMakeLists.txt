add_executable(test_ilist test_ilist.cc)

target_link_libraries(
    test_ilist
    PRIVATE utils
)

add_test(
    NAME test_ilist
    COMMAND test_ilist
)
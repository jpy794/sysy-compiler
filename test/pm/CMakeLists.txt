add_executable(
    test_pm
    test_pm.cc
)


target_link_libraries(
    test_pm
    PRIVATE ir
    PRIVATE pass
    PRIVATE utils
)

add_test(
    NAME PM-Sanity-Test
    COMMAND test_pm
)

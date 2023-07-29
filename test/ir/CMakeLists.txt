set(index 0)
file(GLOB TEST_SRC_FILE_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
foreach(test_src ${TEST_SRC_FILE_LIST})
    get_filename_component(test_exe_name ${test_src} NAME_WLE)
    add_executable(${test_exe_name} ${test_src})
    target_link_libraries(
        ${test_exe_name}
        PUBLIC ir
        PRIVATE utils
    )

    math(EXPR index "${index} + 1")
    add_test(
        NAME "ir-test-${index}"
        COMMAND ${test_exe_name}
    )

endforeach()

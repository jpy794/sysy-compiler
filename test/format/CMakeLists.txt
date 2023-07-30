add_custom_target(
    check_format
    COMMAND find ${CMAKE_SOURCE_DIR} -name '*.hh' -or -name '*.cc' | xargs clang-format -n -Werror
)

add_custom_target(
    format
    COMMAND find ${CMAKE_SOURCE_DIR} -name '*.hh' -or -name '*.cc' | xargs clang-format -i -Werror
)

add_library(
    pass
    pass.cc
    pass.hh
)

target_include_directories(
    pass
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    pass
    PRIVATE ir
    PRIVATE utils
)

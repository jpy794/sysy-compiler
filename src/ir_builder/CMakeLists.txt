add_library(
    ir_builder
    ir_builder.cc
    ir_builder.hh
)

target_include_directories(
    ir_builder
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ir_builder
    PRIVATE ast
    PRIVATE ir
    PRIVATE utils
)

add_library(
    mir_builder
    mir_builder.cc
    mir_builder.hh
)

target_include_directories(
    mir_builder
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    mir_builder
    PRIVATE ir
    PRIVATE mir
    PRIVATE utils
)

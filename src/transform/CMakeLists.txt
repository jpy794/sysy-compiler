file(GLOB OPT_SRC_FILES *.cc *.hh)

add_library(transform ${OPT_SRC_FILES})

target_include_directories(
    transform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    transform
    PRIVATE pass
    PRIVATE utils
    PRIVATE ir
    PRIVATE analysis
)

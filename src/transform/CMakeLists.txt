file(GLOB OPT_SRC_FILES *.cc *.hh)

add_library(opt ${OPT_SRC_FILES})

target_include_directories(
    opt
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    opt
    PRIVATE pass
    PRIVATE utils
    PRIVATE ir
    PRIVATE analysis
)

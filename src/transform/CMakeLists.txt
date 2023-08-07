file(GLOB OPT_SRC_FILES *.cc *.hh)

SET(EXCLUDE_FILES "${CMAKE_CURRENT_LIST_DIR}/loop_invariant.cc" "${CMAKE_CURRENT_LIST_DIR}/loop_invariant.hh" "${CMAKE_CURRENT_LIST_DIR}/loop_unroll.cc" "${CMAKE_CURRENT_LIST_DIR}/loop_unroll.hh")

list(REMOVE_ITEM OPT_SRC_FILES ${EXCLUDE_FILES})

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

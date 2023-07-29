file(GLOB ANA_SRC_FILES *.cc *.hh)

add_library(analysis ${ANA_SRC_FILES})

target_include_directories(
    analysis
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    analysis
    PRIVATE pass
    PRIVATE utils
    PRIVATE ir
    PRIVATE transform
)

file(GLOB IR_SRC_FILES *.cc *.hh)

add_library(ir ${IR_SRC_FILES})

target_include_directories(
    ir 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ir
    PRIVATE utils
)

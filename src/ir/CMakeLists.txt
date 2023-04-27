file(GLOB IR_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

add_library(ir ${IR_SRC_FILES})

target_include_directories(
    ir 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

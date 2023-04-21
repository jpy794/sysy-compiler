set(IR_lib_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(IR_lib_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB IR_lib_src_files ${IR_lib_src_dir}/*.cpp)

add_library(IR_lib ${IR_lib_src_files})

target_include_directories(
    IR_lib 
    PUBLIC ${IR_lib_include_dir}
)

# add_subdirectory(${IR_lib_src_dir})

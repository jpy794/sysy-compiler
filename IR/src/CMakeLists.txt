set(IR_lib_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB IR_lib_src_files ${IR_lib_src_dir}/*.cpp)

add_library(IR_lib ${IR_lib_src_files})

target_include_directories(IR_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

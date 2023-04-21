cmake_minimum_required(VERSION 3.7)
project(sysy-compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
list(APPEND CMAKE_CXX_FLAGS "-Wall -Werror")

add_subdirectory(src)

add_executable(sysyc main.cc)

target_link_libraries(
    sysyc
    ast
)

cmake_minimum_required(VERSION 3.7)
project(sysy-compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")

set(CMAKE_CTEST_ARGUMENTS "${CMAKE_CTEST_ARGUMENTS} --output-on-failure")
enable_testing()

add_subdirectory(src)
add_subdirectory(test)

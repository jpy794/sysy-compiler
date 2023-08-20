cmake_minimum_required(VERSION 3.7)
project(sysy-compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()


list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
enable_testing()

add_subdirectory(src)
add_subdirectory(test)

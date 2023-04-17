# antlr executable file path
set(ANTLR4_JAR_LOCATION /usr/share/java/antlr-complete.jar)

# ref https://github.com/antlr/antlr4/blob/master/runtime/Cpp/cmake/Antlr4Package.md
find_package(antlr4-runtime REQUIRED)
find_package(antlr4-generator REQUIRED)

antlr4_generate(sysy ${CMAKE_CURRENT_SOURCE_DIR}/sysy.g4 BOTH)

add_library(parser ${ANTLR4_SRC_FILES_sysy})

target_include_directories(
    parser
    PUBLIC ${ANTLR4_INCLUDE_DIR}
    PUBLIC ${ANTLR4_INCLUDE_DIR_sysy}
)

target_link_libraries(parser PRIVATE antlr4-runtime)

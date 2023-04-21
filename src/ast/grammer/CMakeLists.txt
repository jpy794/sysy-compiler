# antlr executable file path
if (NOT DEFINED ANTLR4_JAR_LOCATION)
    find_package(Java COMPONENTS Runtime REQUIRED)
    include(UseJava)
    find_jar(
        ANTLR4_JAR
        NAMES antlr-complete
        PATHS /usr/share/java
    )
    set(ANTLR4_JAR_LOCATION ${ANTLR4_JAR})
endif ()

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

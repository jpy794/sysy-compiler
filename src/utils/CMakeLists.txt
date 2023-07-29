add_library(
    utils
    err.hh
    ilist.hh
    utils.hh
    utils.cc
    hash.hh
    log.hh
    log.cc
)

# currently there's no source file in utils
# we have to specify linker language manually
set_target_properties(
    utils
    PROPERTIES LINKER_LANGUAGE CXX
)

target_include_directories(
    utils
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

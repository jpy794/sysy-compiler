add_library(builder
    sysy_builder.cc
)

target_include_directories(builder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(builder
    PRIVATE
        ast
        ir
        utils
)

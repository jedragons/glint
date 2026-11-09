add_library(glint-config INTERFACE)

set_target_properties(glint-config PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-config INTERFACE
    ./include/
)

install(DIRECTORY include/ DESTINATION include)

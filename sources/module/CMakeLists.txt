add_library(glint-module INTERFACE)

set_target_properties(glint-module PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-module INTERFACE
    ./include/
)

target_link_libraries(glint-module INTERFACE
    EnTT::EnTT
)

install(DIRECTORY include/ DESTINATION include)

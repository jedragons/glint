add_library(glint-math INTERFACE)

set_target_properties(glint-math PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-math INTERFACE
    ./include/
)

install(DIRECTORY include/ DESTINATION include)

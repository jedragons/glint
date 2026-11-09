add_library(glint-core STATIC)

target_sources(glint-core PRIVATE ./src/lib.cpp)

set_target_properties(glint-core PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-core PUBLIC
    ./include/
)

target_link_libraries(glint-core PUBLIC
    glint-vfs
    glint-module
    glint-platform
    glint-math

    fmt
    lua
    sol2
    wtr
    EnTT::EnTT
)

target_compile_definitions(glint-core PUBLIC
    GLINT_HOT_RELOAD_ASSETS=1
)

install(TARGETS glint-core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

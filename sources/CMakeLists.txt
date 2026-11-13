# The whole engine is a single library built from modules. Each module is a
# single translation unit: its src/lib.cpp #include-s the module's other .cpp
# files, so only one entry point per module is listed here.

add_library(glint STATIC)

target_sources(glint PRIVATE
    ./core/src/lib.cpp
    ./glint/src/lib.cpp
    ./module-audio/src/lib.cpp
    ./module-dim2/src/lib.cpp
    ./platform/src/lib.cpp
    ./vfs/src/lib.cpp
)

set_target_properties(glint PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint PUBLIC
    ./config/include/
    ./core/include/
    ./glint/include/
    ./math/include/
    ./module/include/
    ./module-audio/include/
    ./module-dim2/include/
    ./platform/include/
    ./vfs/include/
)

target_link_libraries(glint PUBLIC
    EnTT::EnTT
    Microsoft.GSL::GSL
    fmt
    lua
    nlohmann_json::nlohmann_json
    raylib
    sol2
    wtr
)

target_compile_definitions(glint PUBLIC
    GLINT_HOT_RELOAD_ASSETS=1
)

install(TARGETS glint
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY
    ./config/include/
    ./core/include/
    ./glint/include/
    ./math/include/
    ./module/include/
    ./module-audio/include/
    ./module-dim2/include/
    ./platform/include/
    ./vfs/include/
    DESTINATION include
)

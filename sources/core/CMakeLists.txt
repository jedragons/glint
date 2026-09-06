add_library(glint-core STATIC)

target_sources(glint-core PRIVATE
    ./src/assets.cpp
    ./src/assets/watcher.cpp
    ./src/core.cpp
    ./src/hierarchy.cpp
    ./src/hierarchy/name_map.cpp
    ./src/lua.cpp
    ./src/lua/asset.cpp
    ./src/lua/entity.cpp
    ./src/lua/registry.cpp
    ./src/texture.cpp
    ./src/texture/asset.cpp
    ./src/time.cpp
    ./src/time/bindings.cpp
    ./src/time/stopwatch.cpp
    ./src/time/timer.cpp
    ./src/types.cpp
    ./src/types/bindings.cpp
    ./src/vfs.cpp
    ./src/windowing.cpp
    ./src/windowing/bindings.cpp
    ./src/windowing/window.cpp
)

set_target_properties(glint-core PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-core PUBLIC
    ./include/
)

target_link_libraries(glint-core PUBLIC
    glint-vfs
    glint-module

    fmt
    lua
    raylib
    sol2
    wtr
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

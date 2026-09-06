add_library(glint-module-dim2 STATIC)

target_sources(glint-module-dim2 PRIVATE
    ./src/dim2.cpp
    ./src/physics.cpp
    ./src/physics/bindings/transform.cpp
    ./src/rendering.cpp
    ./src/rendering/asset/atlas.cpp
    ./src/rendering/bindings/animated_sprite.cpp
    ./src/rendering/bindings/atlas.cpp
    ./src/rendering/bindings/sprite.cpp
    ./src/rendering/ecs/animated_sprite.cpp
    ./src/rendering/ecs/sprite.cpp
)

set_target_properties(glint-module-dim2 PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-module-dim2 PUBLIC
    ./include/
)

target_link_libraries(glint-module-dim2 PUBLIC
    glint-core
    glint-module

    nlohmann_json::nlohmann_json
    raylib
    sol2
)

install(TARGETS glint-module-dim2
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

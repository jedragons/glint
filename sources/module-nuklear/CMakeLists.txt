add_library(glint-module-nuklear STATIC)

target_sources(glint-module-nuklear PRIVATE
    ./src/bindings/ui.cpp
    ./src/nuklear.cpp
)

set_target_properties(glint-module-nuklear PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-module-nuklear PUBLIC
    ./include/
)

target_link_libraries(glint-module-nuklear PUBLIC
    glint-core
    glint-module
    glint-module-dim2

    raylib_nuklear
    sol2
)

install(TARGETS glint-module-nuklear
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

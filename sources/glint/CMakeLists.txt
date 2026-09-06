add_library(glint STATIC)

target_sources(glint PRIVATE
    ./src/glint.cpp
    ./src/config.cpp
)

target_include_directories(glint PUBLIC
    ./include/
)

set_target_properties(glint PROPERTIES
    CXX_STANDARD 23
)

target_link_libraries(glint PUBLIC
    glint-core
    glint-module
    glint-module-dim2
    glint-module-nuklear
    glint-vfs
    raylib
)

install(TARGETS glint
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

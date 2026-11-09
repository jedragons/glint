add_library(glint-platform STATIC)

target_sources(glint-platform PRIVATE ./src/lib.cpp)

set_target_properties(glint-platform PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-platform PUBLIC
    ./include/
)

target_link_libraries(glint-platform PRIVATE
    glint-config
    glint-math

    fmt
    raylib
)

install(TARGETS glint-platform
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

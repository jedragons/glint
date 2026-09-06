add_library(glint-module-audio STATIC)

target_sources(glint-module-audio PRIVATE
    ./src/audio.cpp
)

set_target_properties(glint-module-audio PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-module-audio PUBLIC
    ./include/
)

target_link_libraries(glint-module-audio PUBLIC
    glint-module

    raylib
)

install(TARGETS glint-module-audio
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

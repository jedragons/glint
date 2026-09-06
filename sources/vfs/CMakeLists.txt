add_library(glint-vfs STATIC)

target_sources(glint-vfs PRIVATE
    ./src/native_filesystem.cpp
    ./src/vfs.cpp
)

set_target_properties(glint-vfs PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-vfs PUBLIC
    ./include/
)

install(TARGETS glint-vfs
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

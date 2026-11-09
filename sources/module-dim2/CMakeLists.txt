add_library(glint-module-dim2 STATIC)

target_sources(glint-module-dim2 PRIVATE ./src/lib.cpp)

set_target_properties(glint-module-dim2 PROPERTIES
    CXX_STANDARD 23
)

target_include_directories(glint-module-dim2 PUBLIC
    ./include/
)

target_link_libraries(glint-module-dim2 PUBLIC
    glint-core
    glint-math
    glint-module

    nlohmann_json::nlohmann_json
    sol2
)

install(TARGETS glint-module-dim2
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

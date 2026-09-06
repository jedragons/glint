add_subdirectory(./entt/)

# EnTT's own ENTT_INSTALL path needs cmake/modules/JoinPaths.cmake and
# cmake/in/*.in templates that aren't part of this vendored copy, so install
# its (header-only) public headers ourselves instead of enabling that option.
install(DIRECTORY entt/src/entt DESTINATION include)

set(FMT_INSTALL ON)
add_subdirectory(./fmt/)

set(JSON_Install ON)
add_subdirectory(./json/)

add_subdirectory(./lua/)
add_subdirectory(./raylib/)
add_subdirectory(./raylib-nuklear/)
add_subdirectory(./sol2/)

option(BUILD_LIB        "Create targets for the watcher-c libraries" ON)
option(BUILD_BIN        "Create targets for the CLI binaries" ON)
option(BUILD_PKG_CONFIG "Create targets for the pkg-config files" ON)
add_subdirectory(./watcher/)

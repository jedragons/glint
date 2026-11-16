cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(GLINT_BUILD_TESTS "Build Glint tests" OFF)

project(Glint VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/GlintCompiler.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "Build the bundled GLFW with Wayland support" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "Build the bundled GLFW with X11 support" FORCE)
endif()

add_subdirectory(vendor/lua)
add_subdirectory(vendor/raylib)

if(GLINT_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(sources)
add_subdirectory(examples)
if(GLINT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

configure_package_config_file(cmake/GlintConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/GlintConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Glint")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/GlintConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT GlintTargets NAMESPACE Glint:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Glint")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GlintConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GlintConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Glint")

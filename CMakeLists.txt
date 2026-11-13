cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(GLINT_BUILD_C_TESTS "Build tests for the C engine" OFF)

project(Glint VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(vendor/lua)
add_subdirectory(vendor/raylib)

if(GLINT_BUILD_C_TESTS)
    enable_testing()
endif()
add_subdirectory(sources)

configure_package_config_file(cmake/GlintConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/GlintConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Glint")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/GlintConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT GlintTargets NAMESPACE Glint:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Glint")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GlintConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/GlintConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Glint")

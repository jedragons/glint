add_library(glint STATIC
    glint/lib.c
    glint/platform/lib.c
    glint/platform/native.c
)
add_library(Glint::glint ALIAS glint)

glint_require_c11(glint PUBLIC)

target_include_directories(glint PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(glint PUBLIC
    $<BUILD_INTERFACE:lua>
    $<INSTALL_INTERFACE:Glint::lua>
)

target_link_libraries(glint PRIVATE
    $<BUILD_INTERFACE:raylib>
    $<INSTALL_INTERFACE:raylib>
)

if(NOT WIN32)
    target_link_libraries(glint PUBLIC m)
endif()

install(TARGETS glint
  EXPORT GlintTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY glint/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/glint"
    FILES_MATCHING PATTERN "*.h"
    PATTERN "script_internal.h" EXCLUDE
    PATTERN "dim2_internal.h" EXCLUDE
    PATTERN "names_internal.h" EXCLUDE
    PATTERN "raylib_internal.h" EXCLUDE
    PATTERN "asset.h" EXCLUDE
    PATTERN "win32.h" EXCLUDE
)

add_library(glint_main STATIC glint/main.c)
add_library(Glint::main ALIAS glint_main)

set_target_properties(glint_main PROPERTIES
    EXPORT_NAME main
)

target_link_libraries(glint_main PUBLIC
    glint
)

install(TARGETS glint_main
    EXPORT GlintTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

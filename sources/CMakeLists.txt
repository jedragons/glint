add_library(glint STATIC
    core/src/strings.c
    core/src/alloc.c
    core/src/arena.c
    window/src/log.c
    window/src/window_lua.c
    types/src/types_lua.c
    types/src/module.c
    module-audio/src/c_audio.c
    module-audio/src/module.c
    names/src/names.c
    glint/src/game.c
    module-dim2/src/c_dim2.c
    module-dim2/src/c_atlas.c
    module-dim2/src/module.c
    runtime/src/modules.c
    time/src/time.c
    time/src/time_lua.c
    time/src/module.c
    script/src/behaviors.c
    script/src/script.c
    script/src/script_entity.c
    vfs/src/vfs.c
    json/src/json.c
    assets/src/cache.c
    ecs/src/ecs.c
    assets/src/file_watcher.c
    render/src/render.c
)
# Keep graphics objects separate so headless tests can provide raylib stubs.
set_source_files_properties(glint/src/game.c module-dim2/src/c_dim2.c module-dim2/src/module.c types/src/module.c time/src/module.c
    window/src/window_lua.c window/src/log.c module-audio/src/c_audio.c module-audio/src/module.c
    render/src/render.c
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
add_library(Glint::glint ALIAS glint)
set_target_properties(glint PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
target_compile_features(glint PUBLIC c_std_11)
set(glint_include_dirs
    core/include window/include types/include module-audio/include
    names/include glint/include module-dim2/include
    runtime/include time/include script/include
    vfs/include json/include ecs/include
    assets/include render/include
)
foreach(dir IN LISTS glint_include_dirs)
    target_include_directories(glint PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${dir}>")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/glint"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endforeach()
target_include_directories(glint PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_link_libraries(glint PUBLIC
    $<BUILD_INTERFACE:lua> $<BUILD_INTERFACE:raylib>
    $<INSTALL_INTERFACE:Glint::lua> $<INSTALL_INTERFACE:raylib>)
if(NOT WIN32)
    target_link_libraries(glint PUBLIC m)
endif()
install(TARGETS glint EXPORT GlintTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

add_library(glint_main STATIC glint/src/main.c)
add_library(Glint::main ALIAS glint_main)
set_target_properties(glint_main PROPERTIES EXPORT_NAME main)
target_link_libraries(glint_main PUBLIC glint)
install(TARGETS glint_main EXPORT GlintTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

if(GLINT_BUILD_C_TESTS)
    function(glint_c_test name source)
        add_executable(glint_c_${name}_test ${source})
        target_link_libraries(glint_c_${name}_test PRIVATE glint)
        target_include_directories(glint_c_${name}_test PRIVATE ${PROJECT_SOURCE_DIR}/tests/support)
        add_test(NAME glint_c_${name} COMMAND glint_c_${name}_test ${ARGN})
    endfunction()

    glint_c_test(window_lua window/tests/window_lua_test.c)
    glint_c_test(types_lua types/tests/types_lua_test.c)
    glint_c_test(types types/tests/types_test.c)
    glint_c_test(render render/tests/render_test.c)
    glint_c_test(asset_cache assets/tests/cache_test.c)
    glint_c_test(audio module-audio/tests/audio_test.c)
    glint_c_test(names names/tests/names_test.c)
    glint_c_test(behaviors script/tests/behaviors_test.c)
    glint_c_test(script_lifecycle script/tests/script_test.c)
    glint_c_test(dim2 module-dim2/tests/dim2_test.c ${PROJECT_SOURCE_DIR}/examples/2d/atlas)
    glint_c_test(game glint/tests/game_test.c ${PROJECT_SOURCE_DIR}/examples/basic)
    glint_c_test(game_window glint/tests/game_window_test.c
        ${PROJECT_SOURCE_DIR}/examples/basic ${PROJECT_SOURCE_DIR}/examples/2d/atlas)
    if(NOT EMSCRIPTEN)
        # Only this object uses the simulated web API; the installed library and
        # other tests must continue to use the native game implementation.
        add_library(glint_c_web_game OBJECT glint/src/game.c)
        target_link_libraries(glint_c_web_game PRIVATE glint)
        target_compile_definitions(glint_c_web_game PRIVATE __EMSCRIPTEN__)
        target_include_directories(glint_c_web_game PRIVATE glint/tests/web_stub)
        glint_c_test(game_web glint/tests/game_window_test.c
            ${PROJECT_SOURCE_DIR}/examples/basic ${PROJECT_SOURCE_DIR}/examples/2d/atlas)
        target_sources(glint_c_game_web_test PRIVATE $<TARGET_OBJECTS:glint_c_web_game>)
        target_compile_definitions(glint_c_game_web_test PRIVATE GGAME_TEST_WEB)
    endif()
    glint_c_test(atlas module-dim2/tests/atlas_test.c)
    glint_c_test(strings core/tests/strings_test.c)
    glint_c_test(time_lua time/tests/time_lua_test.c)
    glint_c_test(modules runtime/tests/modules_test.c)
    if(UNIX AND NOT EMSCRIPTEN)
        # The plugin deliberately resolves ECS calls from the host rather than
        # embedding a second copy of the engine.
        add_library(glint_c_shared_module MODULE runtime/tests/shared_module.c)
        target_compile_features(glint_c_shared_module PRIVATE c_std_11)
        target_include_directories(glint_c_shared_module PRIVATE core/include ecs/include runtime/include)
        if(APPLE)
            target_link_options(glint_c_shared_module PRIVATE "-undefined" "dynamic_lookup")
        endif()
        add_executable(glint_c_shared_module_host runtime/tests/shared_module_host.c)
        set_target_properties(glint_c_shared_module_host PROPERTIES ENABLE_EXPORTS ON)
        target_link_libraries(glint_c_shared_module_host PRIVATE glint ${CMAKE_DL_LIBS})
        # The fixture executes its checks inside assert, including in Release.
        target_compile_options(glint_c_shared_module_host PRIVATE -UNDEBUG)
        add_test(NAME glint_c_shared_module COMMAND glint_c_shared_module_host $<TARGET_FILE:glint_c_shared_module>)
    endif()
    glint_c_test(time time/tests/time_test.c)
    glint_c_test(script ../tests/c/script_test.c)
    glint_c_test(ecs ecs/tests/ecs_test.c)
    glint_c_test(allocator ecs/tests/allocator_test.c)
    glint_c_test(watcher ../tests/file_watcher_test.c)
    glint_c_test(json ../tests/json_test.c)
    glint_c_test(vfs ../tests/c/vfs_test.c ${PROJECT_SOURCE_DIR}/examples/basic)

    if(NOT EMSCRIPTEN)
        function(glint_example_entry_test name source web)
            set(entry_target glint_c_example_${name}_entry)
            add_library(${entry_target} OBJECT glint/src/main.c "${PROJECT_SOURCE_DIR}/examples/${source}/main.c")
            target_include_directories(${entry_target} PRIVATE "${PROJECT_SOURCE_DIR}/examples/tests/stubs")
            target_compile_definitions(${entry_target} PRIVATE main=example_entry ASSETS_DIR="test-assets")
            add_executable(${entry_target}_test "${PROJECT_SOURCE_DIR}/examples/tests/entry_test.c"
                $<TARGET_OBJECTS:${entry_target}>)
            target_include_directories(${entry_target}_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/tests/stubs")
            if(web)
                target_compile_definitions(${entry_target} PRIVATE __EMSCRIPTEN__)
                target_compile_definitions(${entry_target}_test PRIVATE __EMSCRIPTEN__)
            endif()
            # The driver uses assert to both execute and validate its checks.
            if(MSVC)
                target_compile_options(${entry_target}_test PRIVATE /UNDEBUG)
            else()
                target_compile_options(${entry_target}_test PRIVATE -UNDEBUG)
            endif()
            add_test(NAME ${entry_target} COMMAND ${entry_target}_test)
        endfunction()
        glint_example_entry_test(basic basic FALSE)
        glint_example_entry_test(atlas 2d/atlas FALSE)
        glint_example_entry_test(basic_web basic TRUE)
        glint_example_entry_test(atlas_web 2d/atlas TRUE)
    endif()

    # A separate configure must use the same compiler and target architecture.
    # Write an initial cache rather than passing values through CTest arguments:
    # several settings (notably OSX_ARCHITECTURES) can contain semicolons.
    set(package_settings "${CMAKE_CURRENT_BINARY_DIR}/package-test-settings.cmake")
    file(WRITE "${package_settings}" "# Settings for the installed-package consumer.\n")
    set(package_cache_vars
        CMAKE_TOOLCHAIN_FILE CMAKE_GENERATOR_INSTANCE CMAKE_MAKE_PROGRAM
        CMAKE_BUILD_TYPE CMAKE_MSVC_RUNTIME_LIBRARY
        CMAKE_C_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS
        CMAKE_C_COMPILER_TARGET CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN
        CMAKE_SYSROOT CMAKE_SYSROOT_COMPILE CMAKE_SYSROOT_LINK
        CMAKE_FIND_ROOT_PATH CMAKE_FIND_ROOT_PATH_MODE_PACKAGE
        CMAKE_FIND_ROOT_PATH_MODE_LIBRARY CMAKE_FIND_ROOT_PATH_MODE_INCLUDE
        CMAKE_OSX_ARCHITECTURES CMAKE_OSX_SYSROOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(package_configurations Debug Release RelWithDebInfo MinSizeRel ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
    foreach(config IN LISTS package_configurations)
        string(TOUPPER "${config}" config_upper)
        foreach(flag CMAKE_C_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS)
            list(APPEND package_cache_vars "${flag}_${config_upper}")
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES package_cache_vars)
    if(NOT CMAKE_GENERATOR MATCHES "^(Visual Studio|Xcode)")
        list(APPEND package_cache_vars CMAKE_C_COMPILER)
    endif()
    if(CMAKE_CROSSCOMPILING)
        list(APPEND package_cache_vars CMAKE_SYSTEM_NAME CMAKE_SYSTEM_VERSION CMAKE_SYSTEM_PROCESSOR)
    endif()
    foreach(var IN LISTS package_cache_vars)
        if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
            file(APPEND "${package_settings}" "set(${var} [====[${${var}}]====] CACHE STRING \"\" FORCE)\n")
        endif()
    endforeach()
    foreach(var CMAKE_GENERATOR_PLATFORM CMAKE_GENERATOR_TOOLSET CMAKE_VS_PLATFORM_NAME CMAKE_OSX_ARCHITECTURES)
        file(APPEND "${package_settings}" "set(GLINT_OUTER_${var} [====[${${var}}]====] CACHE INTERNAL \"\" FORCE)\n")
    endforeach()

    add_test(NAME glint_c_package COMMAND ${CMAKE_COMMAND}
        "-DGLINT_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DGLINT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DGLINT_CONFIG=$<CONFIG>"
        "-DGLINT_GENERATOR=${CMAKE_GENERATOR}"
        "-DGLINT_SETTINGS=${package_settings}"
        -P "${PROJECT_SOURCE_DIR}/tests/packaging/test_install.cmake")
endif()

function(glint_example name dir)
    set(assets "${PROJECT_SOURCE_DIR}/examples/${dir}")
    add_executable(${name} "${assets}/main.c")
    target_link_libraries(${name} PRIVATE Glint::main)
    if(EMSCRIPTEN)
        target_compile_definitions(${name} PRIVATE ASSETS_DIR="/")
        set_target_properties(${name} PROPERTIES SUFFIX ".html")
        target_link_options(${name} PRIVATE
            -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sGROWABLE_ARRAYBUFFERS=0
            -sSTACK_SIZE=1mb --preload-file "${assets}@/")
    else()
        target_compile_definitions(${name} PRIVATE ASSETS_DIR="${assets}")
    endif()
endfunction()

glint_example(glint_c_basic basic)
glint_example(glint_c_atlas 2d/atlas)

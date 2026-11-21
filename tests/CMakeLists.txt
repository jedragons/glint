include(${PROJECT_SOURCE_DIR}/cmake/GlintTests.cmake)

glint_unit_runner_test(unit_runner unit/runner_test.c unit/fatal_test.c unit/check_runner.cmake)

glint_add_test(allocator core/allocator_test.c)
glint_add_test(strings core/strings_test.c)
glint_add_test(ecs ecs/ecs_test.c)
# Balance the smaller example batch with parser and asset-cache cases.
glint_add_test(json json/json_test.c GROUP examples)
glint_add_test(math math/math_test.c)
glint_add_test(vfs vfs/vfs_test.c ARGS ${PROJECT_SOURCE_DIR}/examples/basic)

glint_add_test(logging platform/logging_test.c)
glint_add_test(filesystem platform/filesystem_test.c)
glint_add_test(watcher platform/watcher_test.c)
glint_adapter_test(rendering platform/rendering_test.c)
glint_adapter_test(audio platform/audio_test.c)

glint_add_test(modules runtime/modules_test.c)
glint_shared_module_test(glint_shared_module runtime/shared_module.c runtime/shared_module_host.c)

glint_add_test(asset_cache modules/assets/cache_test.c GROUP examples)
glint_add_test(audio modules/audio/audio_test.c MOCK AUDIO)
glint_add_test(atlas modules/dim2/atlas_test.c GROUP modules)
set(dim2_example "${PROJECT_SOURCE_DIR}/examples/2d/atlas")
glint_add_test(dim2 modules/dim2/dim2_test.c MOCK RENDER ARGS "${dim2_example}" local)
add_test(NAME glint_dim2_shared COMMAND glint_suites_test dim2 "${dim2_example}" shared)
# Both runs rewrite their own copies of the atlas fixtures.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dim2-local" "${CMAKE_CURRENT_BINARY_DIR}/dim2-shared")
set_tests_properties(glint_dim2 PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dim2-local")
set_tests_properties(glint_dim2_shared PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dim2-shared")
glint_add_test(names modules/names/names_test.c GROUP modules)
glint_add_test(render modules/render/render_test.c MOCK RENDER)
glint_add_test(behaviors modules/script/behaviors_test.c GROUP modules)
glint_add_test(script_lifecycle modules/script/script_test.c GROUP modules)
glint_add_test(script modules/script/script_api_test.c GROUP modules)
glint_add_test(time_bindings modules/time/bindings_test.c GROUP modules)
glint_add_test(time modules/time/time_test.c GROUP modules)
glint_add_test(types_bindings modules/types/bindings_test.c GROUP modules)
glint_add_test(window_bindings modules/window/bindings_test.c MOCK WINDOW)

glint_add_test(game game_test.c ARGS ${PROJECT_SOURCE_DIR}/examples/basic)
glint_platform_test(platform_integration game_window_test.c ARGS
    ${PROJECT_SOURCE_DIR}/examples/basic ${PROJECT_SOURCE_DIR}/examples/2d/atlas)
glint_web_test(game_web game_window_test.c ARGS
    ${PROJECT_SOURCE_DIR}/examples/basic ${PROJECT_SOURCE_DIR}/examples/2d/atlas)

glint_example_entry_test(basic basic)
glint_example_entry_test(basic_web basic WEB)
glint_example_entry_test(atlas 2d/atlas SMOKE)

glint_package_test(glint_package "${PROJECT_SOURCE_DIR}/tests/packaging/test_install.cmake")

glint_finish_tests()

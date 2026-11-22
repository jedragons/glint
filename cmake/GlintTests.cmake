include(${CMAKE_CURRENT_LIST_DIR}/GlintExampleTests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/GlintSharedModuleTest.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/GlintPackageTest.cmake)

set(GLINT_TEST_AUDIO_METHODS audio_ready audio_open audio_close)
set(GLINT_TEST_RENDER_METHODS begin_drawing end_drawing texture_load texture_valid
    texture_unload draw_texture clear_background)
set(GLINT_TEST_WINDOW_METHODS run_loop loop_running cancel_loop window_open window_close
    window_should_close frame_time time width height)
set(GLINT_TEST_PLATFORM_METHODS ${GLINT_TEST_AUDIO_METHODS} ${GLINT_TEST_RENDER_METHODS} ${GLINT_TEST_WINDOW_METHODS})
set(GLINT_TEST_RAYLIB_SYMBOLS InitWindow IsWindowReady CloseWindow SetConfigFlags SetTargetFPS
    SetTraceLogLevel SetTraceLogCallback WindowShouldClose GetFrameTime GetTime GetScreenWidth GetScreenHeight
    BeginDrawing EndDrawing ClearBackground TraceLog IsAudioDeviceReady SetAudioStreamBufferSizeDefault
    InitAudioDevice CloseAudioDevice LoadImageFromMemory IsImageValid UnloadImage LoadTextureFromImage
    IsTextureValid UnloadTexture DrawTexturePro)
set(GLINT_TEST_ADAPTER_SYMBOLS graylib_init_logging graylib__log gplatform__to_raylib_texture
    gplatform__from_raylib_texture gplatform__loop gplatform__loop_frame)
set(GLINT_TEST_GAME_SYMBOLS glint__RaylibState glint__WindowContext glint__test_state glint__test_asset_root
    glint__test_extra_root glint__test_context_create glint__test_context_free glint__test_window_create
    glint__test_web_create glint__test_capture_log glint__test_reset_raylib glint__test_browser_tick
    glint__test_check_loop_allocator glint__test_end_and_destroy glint__test_fail_frame
    glint__test_failed_and_destroy glint__test_record_completion)

# Keep the engine suites in five objects. The runner self-tests build separately.
add_executable(glint_suites_test
    engine.c
    "${CMAKE_CURRENT_BINARY_DIR}/core.c"
    "${CMAKE_CURRENT_BINARY_DIR}/modules.c"
    "${CMAKE_CURRENT_BINARY_DIR}/platform.c"
    "${CMAKE_CURRENT_BINARY_DIR}/examples.c"
)
# These sources already form balanced unity batches; keep OS and raylib headers apart.
set_target_properties(glint_suites_test PROPERTIES UNITY_BUILD OFF)
glint_require_c11(glint_suites_test PRIVATE)
target_include_directories(glint_suites_test PRIVATE
    ${PROJECT_SOURCE_DIR}/sources
    ${PROJECT_SOURCE_DIR}/vendor/raylib/src
    ${PROJECT_SOURCE_DIR}/tests/platform/web_stub
    ${PROJECT_SOURCE_DIR}/tests
)
target_link_libraries(glint_suites_test PRIVATE lua ${CMAKE_DL_LIBS})
if(NOT WIN32)
    target_link_libraries(glint_suites_test PRIVATE m)
endif()

function(glint_test_suite name source)
    cmake_parse_arguments(PARSE_ARGV 2 SUITE "" "GROUP;PRELUDE;POSTLUDE" "DEFINES;SYMBOLS;BACKEND;AFTER")
    if(NOT SUITE_GROUP)
        set(SUITE_GROUP core)
    endif()
    set(prefix "gtest__${name}_")
    string(CONCAT code
        "static gtest__Suite ${prefix}suite;\n"
        "static int ${prefix}main(int argc, char** argv);\n"
        "#define GTEST_SUITE ${prefix}suite\n"
        "#define GTEST_PREFIX ${prefix}\n"
        "#define main ${prefix}main\n"
        "${SUITE_PRELUDE}\n"
    )
    foreach(define IN LISTS SUITE_DEFINES)
        string(REPLACE "=" " " definition "${define}")
        string(APPEND code "#define ${definition}\n")
    endforeach()
    if(SUITE_BACKEND)
        foreach(method IN LISTS SUITE_BACKEND)
            string(APPEND code "#define gplatform_${method} ${prefix}${method}\n")
        endforeach()
        string(APPEND code "GTEST_PLATFORM_FUNCTIONS(GTEST_PLATFORM_DECLARE)\n")
    endif()
    foreach(symbol IN LISTS SUITE_SYMBOLS)
        string(APPEND code "#define ${symbol} ${prefix}${symbol}\n")
    endforeach()
    string(APPEND code "#include \"${CMAKE_CURRENT_SOURCE_DIR}/${source}\"\n")
    foreach(implementation IN LISTS SUITE_AFTER)
        string(APPEND code "#include \"${PROJECT_SOURCE_DIR}/sources/glint/platform/${implementation}.c\"\n")
    endforeach()
    string(APPEND code "${SUITE_POSTLUDE}\n#undef main\n")
    foreach(symbol IN LISTS SUITE_SYMBOLS)
        string(APPEND code "#undef ${symbol}\n")
    endforeach()
    foreach(method IN LISTS SUITE_BACKEND)
        string(APPEND code "#undef gplatform_${method}\n")
    endforeach()
    foreach(define IN LISTS SUITE_DEFINES)
        string(REGEX REPLACE "=.*" "" macro "${define}")
        string(APPEND code "#undef ${macro}\n")
        if(macro STREQUAL "GLINT_PLATFORM_EMSCRIPTEN")
            string(APPEND code "#define GLINT_PLATFORM_EMSCRIPTEN 0\n")
        endif()
    endforeach()
    string(APPEND code
        "#undef GTEST_PREFIX\n#undef GTEST_SUITE\n"
        "int gtest_${name}_run(int argc, char** argv) {\n"
        "    gtest_platform = (gtest_Platform) {0};\n"
    )
    foreach(method IN LISTS SUITE_BACKEND)
        string(APPEND code "    gtest_platform.${method} = ${prefix}${method};\n")
    endforeach()
    string(APPEND code "    return ${prefix}main(argc, argv);\n}\n\n")
    set_property(GLOBAL APPEND_STRING PROPERTY GLINT_TEST_${SUITE_GROUP}_SOURCES "${code}")
    set_property(GLOBAL APPEND_STRING PROPERTY GLINT_TEST_DECLARATIONS
        "int gtest_${name}_run(int argc, char** argv);\n")
    set_property(GLOBAL APPEND_STRING PROPERTY GLINT_TEST_COMMANDS
        "    if (strcmp(argv[1], \"${name}\") == 0) return gtest_${name}_run(argc - 1, argv + 1);\n")
endfunction()

function(glint_add_test name source)
    cmake_parse_arguments(TEST "" "MOCK;GROUP" "ARGS" ${ARGN})
    if(NOT TEST_GROUP)
        set(TEST_GROUP core)
    endif()
    set(backend)
    if(TEST_MOCK STREQUAL "AUDIO")
        set(backend ${GLINT_TEST_AUDIO_METHODS})
    elseif(TEST_MOCK STREQUAL "RENDER")
        set(backend texture_load texture_valid texture_unload draw_texture clear_background)
    elseif(TEST_MOCK STREQUAL "WINDOW")
        set(backend frame_time time width height)
    endif()
    if(TEST_MOCK)
        set(TEST_GROUP modules)
    endif()
    glint_test_suite(${name} ${source} GROUP "${TEST_GROUP}" BACKEND ${backend})
    add_test(NAME glint_${name} COMMAND glint_suites_test ${name} ${TEST_ARGS})
endfunction()

function(glint_unit_runner_test name source fatal_source script)
    # Exercise the public header's own registry outside the batched runner.
    add_executable(glint_${name}_test ${source})
    add_executable(glint_${name}_fatal_test ${fatal_source})
    foreach(target glint_${name}_test glint_${name}_fatal_test)
        glint_require_c11(${target} PRIVATE)
        target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/sources)
    endforeach()
    add_test(NAME glint_${name} COMMAND ${CMAKE_COMMAND}
        "-DRUNNER=$<TARGET_FILE:glint_${name}_test>"
        "-DFATAL_RUNNER=$<TARGET_FILE:glint_${name}_fatal_test>"
        "-DEMULATOR=$<TARGET_PROPERTY:glint_${name}_test,CROSSCOMPILING_EMULATOR>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}")
endfunction()

function(glint_platform_test name source)
    cmake_parse_arguments(TEST "" "" "ARGS" ${ARGN})
    glint_test_suite(${name} ${source} GROUP platform
        BACKEND ${GLINT_TEST_PLATFORM_METHODS}
        SYMBOLS ${GLINT_TEST_RAYLIB_SYMBOLS} ${GLINT_TEST_GAME_SYMBOLS} ${GLINT_TEST_ADAPTER_SYMBOLS}
        DEFINES GLINT_TEST_PLATFORM
        AFTER raylib audio rendering windowing)
    add_test(NAME glint_${name} COMMAND glint_suites_test ${name} ${TEST_ARGS})
endfunction()

function(glint_web_test name source)
    cmake_parse_arguments(TEST "" "" "ARGS" ${ARGN})
    glint_test_suite(${name} ${source} GROUP platform
        BACKEND ${GLINT_TEST_PLATFORM_METHODS}
        SYMBOLS ${GLINT_TEST_RAYLIB_SYMBOLS} ${GLINT_TEST_GAME_SYMBOLS} ${GLINT_TEST_ADAPTER_SYMBOLS}
        DEFINES GLINT_TEST_WEB GLINT_PLATFORM_EMSCRIPTEN=1
        PRELUDE "#undef GLINT_PLATFORM_EMSCRIPTEN"
        AFTER raylib audio rendering windowing)
    add_test(NAME glint_${name} COMMAND glint_suites_test ${name} ${TEST_ARGS})
endfunction()

function(glint_adapter_test name source)
    if(name STREQUAL "audio")
        set(backend ${GLINT_TEST_AUDIO_METHODS})
    else()
        set(backend ${GLINT_TEST_RENDER_METHODS})
    endif()
    glint_test_suite(platform_${name} ${source} GROUP platform
        BACKEND ${backend}
        SYMBOLS ${GLINT_TEST_RAYLIB_SYMBOLS} ${GLINT_TEST_ADAPTER_SYMBOLS}
        AFTER raylib ${name})
    add_test(NAME glint_platform_${name} COMMAND glint_suites_test platform_${name})
endfunction()

function(glint_finish_tests)
    foreach(group core modules platform examples)
        set(GLINT_TEST_GROUP_INCLUDES)
        if(group STREQUAL "examples")
            set(GLINT_TEST_GROUP_INCLUDES "#include \"examples/game.h\"")
        endif()
        get_property(GLINT_TEST_SUITE_SOURCES GLOBAL PROPERTY GLINT_TEST_${group}_SOURCES)
        set(GLINT_TEST_MAIN)
        if(group STREQUAL "core")
            get_property(GLINT_TEST_DECLARATIONS GLOBAL PROPERTY GLINT_TEST_DECLARATIONS)
            get_property(GLINT_TEST_COMMANDS GLOBAL PROPERTY GLINT_TEST_COMMANDS)
            string(CONCAT GLINT_TEST_MAIN
                "${GLINT_TEST_DECLARATIONS}\n"
                "int main(int argc, char** argv) {\n"
                "    if (argc < 2) {\n"
                "        fprintf(stderr, \"usage: %s SUITE [ARGS...]\\n\", argv[0]);\n"
                "        return EXIT_FAILURE;\n    }\n"
                "${GLINT_TEST_COMMANDS}"
                "    fprintf(stderr, \"unknown test suite: %s\\n\", argv[1]);\n"
                "    return EXIT_FAILURE;\n}\n"
            )
        endif()
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/suites.c.in" "${CMAKE_CURRENT_BINARY_DIR}/${group}.c" @ONLY)
    endforeach()
endfunction()

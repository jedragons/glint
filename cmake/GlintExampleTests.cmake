function(glint_example_entry_test name source)
    cmake_parse_arguments(PARSE_ARGV 2 EXAMPLE "WEB;SMOKE" "" "")
    set(symbols galloc_malloc_allocator glint_new glint_destroy glint_error
        glint_run_with_completion glint_main example_entry glint__main_done glint__EntryContext
        glint__test_context glint__test_context_create glint__test_context_free)
    set(prelude "#undef GLINT_PLATFORM_EMSCRIPTEN\n#define GLINT_PLATFORM_EMSCRIPTEN 0")
    set(defines ASSETS_DIR="test-assets" glint_Game=gtest_ExampleGame glint_RunEndFn=gtest_ExampleRunEndFn)
    if(EXAMPLE_WEB)
        set(prelude "#undef GLINT_PLATFORM_EMSCRIPTEN")
        list(APPEND defines GLINT_PLATFORM_EMSCRIPTEN=1)
    endif()
    if(EXAMPLE_SMOKE)
        list(APPEND defines GLINT_TEST_EXAMPLE_SMOKE)
    endif()
    string(CONCAT postlude
        "#undef main\n#define main gtest__example_${name}_example_entry\n"
        "#include \"${PROJECT_SOURCE_DIR}/examples/${source}/main.c\"\n"
        "#include \"${PROJECT_SOURCE_DIR}/sources/glint/main.c\"\n"
    )
    glint_test_suite(example_${name} examples/entry_test.c GROUP examples
        SYMBOLS ${symbols} DEFINES ${defines} PRELUDE "${prelude}" POSTLUDE "${postlude}")
    add_test(NAME glint_example_${name}_entry COMMAND glint_suites_test example_${name})
endfunction()

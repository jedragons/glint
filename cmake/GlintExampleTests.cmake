function(glint_example_entry_test name source web)
    if(EMSCRIPTEN)
        return()
    endif()

    set(entry_target glint_example_${name}_entry)

    add_library(${entry_target} OBJECT
        ${PROJECT_SOURCE_DIR}/sources/glint/main.c
        "${PROJECT_SOURCE_DIR}/examples/${source}/main.c"
    )

    target_include_directories(${entry_target} PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/examples/stubs"
        "${PROJECT_SOURCE_DIR}/sources"
    )

    target_compile_definitions(${entry_target} PRIVATE
        main=example_entry
        ASSETS_DIR="test-assets"
    )

    add_executable(${entry_target}_test
        "${PROJECT_SOURCE_DIR}/tests/examples/entry_test.c"
        $<TARGET_OBJECTS:${entry_target}>
    )

    target_include_directories(${entry_target}_test PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/examples/stubs"
        "${PROJECT_SOURCE_DIR}/sources"
    )

    if(web)
        target_compile_definitions(${entry_target} PRIVATE
            GLINT_PLATFORM_EMSCRIPTEN=1
        )

        target_compile_definitions(${entry_target}_test PRIVATE
            GLINT_PLATFORM_EMSCRIPTEN=1
        )
    endif()

    if(MSVC)
        target_compile_options(${entry_target}_test PRIVATE /UNDEBUG)
    else()
        target_compile_options(${entry_target}_test PRIVATE -UNDEBUG)
    endif()

    add_test(NAME ${entry_target} COMMAND ${entry_target}_test)
endfunction()

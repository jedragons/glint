include(${CMAKE_CURRENT_LIST_DIR}/GlintExampleTests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/GlintSharedModuleTest.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/GlintPackageTest.cmake)

add_library(glint_test STATIC
    ${PROJECT_SOURCE_DIR}/sources/glint/lib.c
    ${PROJECT_SOURCE_DIR}/sources/glint/platform/logging.c
    ${PROJECT_SOURCE_DIR}/sources/glint/platform/native.c
)

target_include_directories(glint_test PUBLIC
    ${PROJECT_SOURCE_DIR}/sources
)

target_link_libraries(glint_test PUBLIC
    lua
    raylib
)

if(NOT WIN32)
    target_link_libraries(glint_test PUBLIC m)
endif()

if(NOT EMSCRIPTEN)
    add_library(glint_test_web STATIC
        ${PROJECT_SOURCE_DIR}/sources/glint/lib.c
        ${PROJECT_SOURCE_DIR}/sources/glint/platform/lib.c
        ${PROJECT_SOURCE_DIR}/sources/glint/platform/native.c
    )

    target_compile_definitions(glint_test_web PRIVATE
        GLINT_PLATFORM_EMSCRIPTEN=1
    )

    target_include_directories(glint_test_web
        PUBLIC
            ${PROJECT_SOURCE_DIR}/sources
        PRIVATE
            platform/web_stub
    )

    target_link_libraries(glint_test_web PUBLIC
        lua
        raylib
    )

    if(NOT WIN32)
        target_link_libraries(glint_test_web PUBLIC m)
    endif()
endif()

function(glint_add_test name source)
    cmake_parse_arguments(TEST "" "MOCK" "ARGS" ${ARGN})

    add_executable(glint_${name}_test
        ${source}
        platform/mock.c
    )

    if(TEST_MOCK)
        target_compile_definitions(glint_${name}_test PRIVATE
            GLINT_MOCK_${TEST_MOCK}
        )
    endif()

    target_link_libraries(glint_${name}_test PRIVATE
        glint_test
    )

    add_test(NAME glint_${name} COMMAND glint_${name}_test ${TEST_ARGS})
endfunction()

function(glint_unit_runner_test name source fatal_source script)
    add_executable(glint_${name}_test ${source})
    target_include_directories(glint_${name}_test PRIVATE ${PROJECT_SOURCE_DIR}/sources)
    add_executable(glint_${name}_fatal_test ${fatal_source})
    target_include_directories(glint_${name}_fatal_test PRIVATE ${PROJECT_SOURCE_DIR}/sources)
    add_test(NAME glint_${name} COMMAND ${CMAKE_COMMAND}
        "-DRUNNER=$<TARGET_FILE:glint_${name}_test>"
        "-DFATAL_RUNNER=$<TARGET_FILE:glint_${name}_fatal_test>"
        "-DEMULATOR=$<TARGET_PROPERTY:glint_${name}_test,CROSSCOMPILING_EMULATOR>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}")
endfunction()

# Exercise the separate platform object against raylib test doubles.
function(glint_platform_test name source)
    cmake_parse_arguments(TEST "" "" "ARGS" ${ARGN})

    add_executable(glint_${name}_test ${source})
    target_link_libraries(glint_${name}_test PRIVATE glint raylib)
    target_compile_definitions(glint_${name}_test PRIVATE GLINT_TEST_PLATFORM)
    add_test(NAME glint_${name} COMMAND glint_${name}_test ${TEST_ARGS})
endfunction()

function(glint_web_test name source)
    if(EMSCRIPTEN)
        return()
    endif()

    cmake_parse_arguments(TEST "" "" "ARGS" ${ARGN})

    add_executable(glint_${name}_test ${source})
    target_link_libraries(glint_${name}_test PRIVATE glint_test_web)
    target_compile_definitions(glint_${name}_test PRIVATE GLINT_TEST_WEB)
    add_test(NAME glint_${name} COMMAND glint_${name}_test ${TEST_ARGS})
endfunction()

# Compile the real adapter against backend stand-ins without loading a device or window.
function(glint_adapter_test name source)
    add_executable(glint_platform_${name}_test
        ${source}
        ${PROJECT_SOURCE_DIR}/sources/glint/platform/${name}.c
        ${PROJECT_SOURCE_DIR}/sources/glint/platform/raylib.c
        ${PROJECT_SOURCE_DIR}/sources/glint/platform/logging.c
    )
    target_include_directories(glint_platform_${name}_test PRIVATE ${PROJECT_SOURCE_DIR}/sources)
    target_compile_features(glint_platform_${name}_test PRIVATE c_std_11)
    target_link_libraries(glint_platform_${name}_test PRIVATE raylib)
    add_test(NAME glint_platform_${name} COMMAND glint_platform_${name}_test)
endfunction()

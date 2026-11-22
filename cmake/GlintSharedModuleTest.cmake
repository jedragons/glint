function(glint_shared_module_test name module_source host_source)
    if(NOT UNIX OR EMSCRIPTEN)
        return()
    endif()

    add_library(${name} MODULE
        ${module_source}
    )

    glint_require_c11(${name} PRIVATE)

    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/sources
    )

    if(APPLE)
        target_link_options(${name} PRIVATE
            "-undefined"
            "dynamic_lookup"
        )
    endif()

    glint_test_suite(shared_module ${host_source})
    set_target_properties(glint_suites_test PROPERTIES ENABLE_EXPORTS ON)
    add_test(NAME ${name} COMMAND glint_suites_test shared_module $<TARGET_FILE:${name}>)
endfunction()

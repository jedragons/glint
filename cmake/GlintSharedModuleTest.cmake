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

    add_executable(${name}_host ${host_source})

    set_target_properties(${name}_host PROPERTIES ENABLE_EXPORTS ON)

    target_link_libraries(${name}_host PRIVATE glint ${CMAKE_DL_LIBS})

    target_compile_options(${name}_host PRIVATE -UNDEBUG)

    add_test(NAME ${name} COMMAND ${name}_host $<TARGET_FILE:${name}>)
endfunction()

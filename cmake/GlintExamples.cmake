function(glint_example name dir)
    set(assets "${PROJECT_SOURCE_DIR}/examples/${dir}")

    add_executable(${name} "${assets}/main.c")

    target_link_libraries(${name} PRIVATE Glint::main)

    if(EMSCRIPTEN)
        target_compile_definitions(${name} PRIVATE
            ASSETS_DIR="/"
        )

        set_target_properties(${name} PROPERTIES SUFFIX ".html")

        target_link_options(${name} PRIVATE
            -sUSE_GLFW=3
            -sALLOW_MEMORY_GROWTH=1
            -sGROWABLE_ARRAYBUFFERS=0
            -sSTACK_SIZE=1mb
            --preload-file "${assets}@/"
        )
    else()
        target_compile_definitions(${name} PRIVATE
            ASSETS_DIR="${assets}"
        )
    endif()
endfunction()

function(add_example name dir)
    add_executable(${name})

    target_sources(${name} PRIVATE
        ./${dir}/main.cpp
    )

    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 23
    )


    target_link_libraries(${name} PRIVATE
        glint
        ${ARGN}
    )

    if(EMSCRIPTEN)
        target_compile_definitions(${name} PRIVATE
            ASSETS_DIR="/"
        )

        set_target_properties(${name} PROPERTIES
            SUFFIX ".html"
        )

        target_link_options(${name} PRIVATE
            -sUSE_GLFW=3
            -sALLOW_MEMORY_GROWTH=1
            -sGROWABLE_ARRAYBUFFERS=0
            -sSTACK_SIZE=1mb
            --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/${dir}@/"
        )
    else()
        target_compile_definitions(${name} PRIVATE
            ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${dir}/"
        )
    endif()
endfunction()

add_example(basic basic)
add_example(2d-atlas 2d/atlas)

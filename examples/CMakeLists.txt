function(add_example name dir)
    add_executable(${name})

    target_sources(${name} PRIVATE
        ./${dir}/main.cpp
    )

    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 23
    )

    target_compile_definitions(${name} PRIVATE
        ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${dir}"
    )

    target_link_libraries(${name} PRIVATE
        glint
        ${ARGN}
    )
endfunction()

add_example(basic basic)
add_example(2d-atlas 2d/atlas)
add_example(ui-basic ui/basic)

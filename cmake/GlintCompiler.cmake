if(CMAKE_C_COMPILER_ID STREQUAL "TinyCC" AND NOT CMAKE_DEPFILE_FLAGS_C)
    # CMake omits TCC's dependency flags; TCC does not support GCC's -MT.
    set(CMAKE_DEPFILE_FLAGS_C "-MD -MF <DEP_FILE>")
    set(CMAKE_C_DEPFILE_FORMAT gcc)
    if(NOT DEFINED CMAKE_DEPENDS_USE_COMPILER OR CMAKE_DEPENDS_USE_COMPILER)
        set(CMAKE_C_DEPENDS_USE_COMPILER TRUE)
    endif()
endif()

function(glint_require_c11 target visibility)
    # TinyCC needs an explicit flag; defer selection for installed consumers.
    target_compile_features(${target} ${visibility}
        "$<$<NOT:$<C_COMPILER_ID:TinyCC>>:c_std_11>"
    )
    target_compile_options(${target} ${visibility}
        "$<$<C_COMPILER_ID:TinyCC>:-std=c11>"
    )
endfunction()

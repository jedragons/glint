list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GLINT_TOOLCHAIN_COMPILER)

if(GLINT_TOOLCHAIN_COMPILER STREQUAL "clang")
    set(CMAKE_C_COMPILER /usr/bin/clang CACHE FILEPATH "C compiler")
else()
    if(GLINT_TOOLCHAIN_COMPILER STREQUAL "gcc")
        set(formula gcc)
    else()
        set(formula llvm)
    endif()

    execute_process(COMMAND brew --prefix "${formula}"
        OUTPUT_VARIABLE formula_prefix OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)

    if(GLINT_TOOLCHAIN_COMPILER STREQUAL "gcc")
        file(GLOB gcc_compilers "${formula_prefix}/bin/gcc-[0-9]*")
        list(SORT gcc_compilers COMPARE NATURAL ORDER DESCENDING)
        if(NOT gcc_compilers)
            message(FATAL_ERROR "Homebrew GCC not found in ${formula_prefix}/bin")
        endif()
        list(GET gcc_compilers 0 compiler)
    else()
        set(compiler "${formula_prefix}/bin/clang")
    endif()

    set(CMAKE_C_COMPILER "${compiler}" CACHE FILEPATH "C compiler")
endif()


function(glint_package_test name script)
    set(package_settings
        "${CMAKE_CURRENT_BINARY_DIR}/package-test-settings.cmake"
    )

    file(WRITE "${package_settings}"
        "# Settings for the installed-package consumer.\n"
    )

    set(package_cache_vars
        CMAKE_TOOLCHAIN_FILE
        CMAKE_CROSSCOMPILING_EMULATOR
        CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}
        CMAKE_GENERATOR_INSTANCE
        CMAKE_MAKE_PROGRAM
        CMAKE_BUILD_TYPE
        CMAKE_MSVC_RUNTIME_LIBRARY
        CMAKE_C_FLAGS
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_SHARED_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS
        CMAKE_STATIC_LINKER_FLAGS
        CMAKE_C_COMPILER_TARGET
        CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN
        CMAKE_SYSROOT
        CMAKE_SYSROOT_COMPILE
        CMAKE_SYSROOT_LINK
        CMAKE_FIND_ROOT_PATH
        CMAKE_FIND_ROOT_PATH_MODE_PACKAGE
        CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
        CMAKE_FIND_ROOT_PATH_MODE_INCLUDE
        CMAKE_OSX_ARCHITECTURES
        CMAKE_OSX_SYSROOT
        CMAKE_OSX_DEPLOYMENT_TARGET
    )

    set(package_configurations
        Debug
        Release
        RelWithDebInfo
        MinSizeRel
        ${CMAKE_CONFIGURATION_TYPES}
        ${CMAKE_BUILD_TYPE}
    )

    foreach(config IN LISTS package_configurations)
        string(TOUPPER "${config}" config_upper)
        foreach(flag
            CMAKE_C_FLAGS
            CMAKE_EXE_LINKER_FLAGS
            CMAKE_SHARED_LINKER_FLAGS
            CMAKE_MODULE_LINKER_FLAGS
            CMAKE_STATIC_LINKER_FLAGS
        )
            list(APPEND package_cache_vars "${flag}_${config_upper}")
        endforeach()
    endforeach()

    list(REMOVE_DUPLICATES package_cache_vars)

    if(NOT CMAKE_GENERATOR MATCHES "^(Visual Studio|Xcode)")
        list(APPEND package_cache_vars CMAKE_C_COMPILER)
    endif()

    if(CMAKE_CROSSCOMPILING)
        list(APPEND package_cache_vars
            CMAKE_SYSTEM_NAME
            CMAKE_SYSTEM_VERSION
            CMAKE_SYSTEM_PROCESSOR
        )
    endif()

    foreach(var IN LISTS package_cache_vars)
        if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
            file(APPEND "${package_settings}"
                "set(${var} [====[${${var}}]====] CACHE STRING \"\" FORCE)\n"
            )
        endif()
    endforeach()

    foreach(var
        CMAKE_GENERATOR_PLATFORM
        CMAKE_GENERATOR_TOOLSET
        CMAKE_VS_PLATFORM_NAME
        CMAKE_OSX_ARCHITECTURES
    )
        file(APPEND "${package_settings}"
            "set(GLINT_OUTER_${var} [====[${${var}}]====] CACHE INTERNAL \"\" FORCE)\n"
        )
    endforeach()

    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
        "-DGLINT_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DGLINT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DGLINT_CONFIG=$<CONFIG>"
        "-DGLINT_GENERATOR=${CMAKE_GENERATOR}"
        "-DGLINT_SETTINGS=${package_settings}"
        -P "${script}"
    )
endfunction()

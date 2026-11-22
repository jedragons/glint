list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GLINT_TARGET_ARCH)

if(GLINT_TARGET_ARCH STREQUAL "arm64")
    set(target_processor aarch64)
else()
    set(target_processor x86_64)
endif()

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" host_processor)
if(host_processor MATCHES "^(arm64|aarch64)$")
    set(host_processor aarch64)
elseif(host_processor STREQUAL "amd64")
    set(host_processor x86_64)
endif()

if(NOT CMAKE_HOST_WIN32 OR NOT host_processor STREQUAL target_processor)
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

set(CMAKE_SYSTEM_PROCESSOR "${target_processor}")
if(NOT CMAKE_RC_COMPILER)
    set(CMAKE_RC_COMPILER "${target_processor}-w64-mingw32-windres")
endif()

if(NOT CMAKE_HOST_WIN32 AND target_processor STREQUAL "x86_64")
    find_program(wine_executable NAMES wine wine64)
    if(wine_executable AND (NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR
        OR CMAKE_CROSSCOMPILING_EMULATOR STREQUAL wine_executable))
        # A failed test must not open Wine's graphical crash debugger.
        set(CMAKE_CROSSCOMPILING_EMULATOR
            "${CMAKE_COMMAND};-E;env;WINEDLLOVERRIDES=winedbg.exe=d;${wine_executable}"
            CACHE STRING "Windows test runner" FORCE)
    endif()
endif()

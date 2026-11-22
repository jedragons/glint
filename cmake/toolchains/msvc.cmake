list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GLINT_TARGET_ARCH)

# Ninja uses the developer shell's SDK and libraries; architecture alone cannot select them.
if(NOT "$ENV{VSCMD_ARG_TGT_ARCH}" STREQUAL GLINT_TARGET_ARCH)
    message(FATAL_ERROR
        "Use a Visual Studio developer shell targeting ${GLINT_TARGET_ARCH} (VsDevCmd -arch=${GLINT_TARGET_ARCH}).")
endif()

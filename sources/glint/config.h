#pragma once

#ifndef GLINT_PLATFORM_LINUX
#   if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#   define GLINT_PLATFORM_LINUX 1
#   else
#   define GLINT_PLATFORM_LINUX 0
#   endif
#endif

#ifndef GLINT_PLATFORM_WINDOWS_64
#   if defined(_WIN64)
#   define GLINT_PLATFORM_WINDOWS_64 1
#   else
#   define GLINT_PLATFORM_WINDOWS_64 0
#   endif
#endif

#ifndef GLINT_PLATFORM_MACOS
#   if defined(__APPLE__)
#   define GLINT_PLATFORM_MACOS 1
#   else
#   define GLINT_PLATFORM_MACOS 0
#   endif
#endif

#ifndef GLINT_PLATFORM_WINDOWS
#   if defined(_WIN32)
#   define GLINT_PLATFORM_WINDOWS 1
#   else
#   define GLINT_PLATFORM_WINDOWS 0
#   endif
#endif

#ifndef GLINT_PLATFORM_ANDROID
#   if defined(__ANDROID__)
#   define GLINT_PLATFORM_ANDROID 1
#   else
#   define GLINT_PLATFORM_ANDROID 0
#   endif
#endif

// TODO
#ifndef GLINT_PLATFORM_IOS
#define GLINT_PLATFORM_IOS 0
#endif

#ifndef GLINT_PLATFORM_EMSCRIPTEN
#   if defined(__EMSCRIPTEN__)
#   define GLINT_PLATFORM_EMSCRIPTEN 1
#   else
#   define GLINT_PLATFORM_EMSCRIPTEN 0
#   endif
#endif

#ifndef GLINT_PLATFORM_DESKTOP
#   if GLINT_PLATFORM_LINUX || GLINT_PLATFORM_MACOS || GLINT_PLATFORM_WINDOWS
#   define GLINT_PLATFORM_DESKTOP 1
#   else
#   define GLINT_PLATFORM_DESKTOP 0
#   endif
#endif

#ifndef GLINT_PLATFORM_WEB
#   if GLINT_PLATFORM_EMSCRIPTEN
#   define GLINT_PLATFORM_WEB 1
#   else
#   define GLINT_PLATFORM_WEB 0
#   endif
#endif

#ifndef GLINT_PLATFORM_MOBILE
#   if GLINT_PLATFORM_ANDROID || GLINT_PLATFORM_IOS
#   define GLINT_PLATFORM_MOBILE 1
#   else
#   define GLINT_PLATFORM_MOBILE 0
#   endif
#endif

#ifndef GLINT_COMPILER_CLANG
#   if defined(__clang__)
#   define GLINT_COMPILER_CLANG 1
#   else
#   define GLINT_COMPILER_CLANG 0
#   endif
#endif

#ifndef GLINT_COMPILER_MSVC
#   if defined(_MSC_VER)
#   define GLINT_COMPILER_MSVC 1
#   else
#   define GLINT_COMPILER_MSVC 0
#   endif
#endif

#ifndef GLINT_COMPILER_GNU
#   if defined(__GNUC__)
#   define GLINT_COMPILER_GNU 1
#   else
#   define GLINT_COMPILER_GNU 0
#   endif
#endif

// MSVC checks custom printf format strings with /analyze.
#if GLINT_COMPILER_MSVC
#   include <sal.h>
#   define GLINT_PRINTF_STRING _Printf_format_string_
#else
#   define GLINT_PRINTF_STRING
#endif

// Argument indices start at one; zero means arguments are supplied as va_list.
#if GLINT_COMPILER_CLANG || GLINT_COMPILER_GNU
#   define GLINT_PRINTF_FORMAT(format_index, first_arg) \
        __attribute__((__format__(__printf__, format_index, first_arg)))
#else
#   define GLINT_PRINTF_FORMAT(format_index, first_arg)
#endif

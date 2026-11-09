#pragma once

#ifndef GLINT_PLATFORM_LINUX
#   if __linux__
#   define GLINT_PLATFORM_LINUX 1
#   else
#   define GLINT_PLATFORM_LINUX 0
#   endif
#endif

#ifndef GLINT_PLATFORM_MACOS
#   if __APPLE__
#   define GLINT_PLATFORM_MACOS 1
#   else
#   define GLINT_PLATFORM_MACOS 0
#   endif
#endif

#ifndef GLINT_PLATFORM_WINDOWS
#   if _WIN32
#   define GLINT_PLATFORM_WINDOWS 1
#   else
#   define GLINT_PLATFORM_WINDOWS 0
#   endif
#endif

#ifndef GLINT_PLATFORM_ANDROID
#   if __ANDROID__
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
#   if GLINT_PLATFORM_LINUX || GLINT_PLATFORM_MACOS || GLINT_PLATFORM_WINDOWS
#   define GLINT_PLATFORM_DESKTOP 1
#   else
#   define GLINT_PLATFORM_DESKTOP 0
#   endif
#endif

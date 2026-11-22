#pragma once

#include <glint/platform/audio.h>
#include <glint/platform/rendering.h>
#include <glint/platform/windowing.h>

// Signatures shared by the dispatch table and namespaced fixture declarations.
#define GTEST_PLATFORM_FUNCTIONS(X) \
    X(bool, audio_ready, (void)) \
    X(void, audio_open, (int buffer_size)) \
    X(void, audio_close, (void)) \
    X(void, begin_drawing, (void)) \
    X(void, end_drawing, (void)) \
    X(gplatform_Texture, texture_load, (const char* type, const void* bytes, size_t size)) \
    X(bool, texture_valid, (gplatform_Texture texture)) \
    X(void, texture_unload, (gplatform_Texture texture)) \
    X(void, draw_texture, (gplatform_Texture texture, gplatform_TextureDrawParams params)) \
    X(void, clear_background, (gcolor_Color color)) \
    X(bool, run_loop, (gplatform_FrameFn frame, gplatform_LoopEndFn end, void* user)) \
    X(bool, loop_running, (void)) \
    X(void, cancel_loop, (void* user)) \
    X(bool, window_open, (int width, int height, const char* title, bool vsync, bool resizable, int fps)) \
    X(void, window_close, (void)) \
    X(bool, window_should_close, (void)) \
    X(float, frame_time, (void)) \
    X(double, time, (void)) \
    X(int, width, (void)) \
    X(int, height, (void))

#define GTEST_PLATFORM_FIELD(result, name, params) result(*name) params;

typedef struct gtest_Platform {
    GTEST_PLATFORM_FUNCTIONS(GTEST_PLATFORM_FIELD)
} gtest_Platform;

#undef GTEST_PLATFORM_FIELD

// CTest launches one suite per process; unset methods use the headless defaults.
extern gtest_Platform gtest_platform;

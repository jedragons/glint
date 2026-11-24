#include "./windowing.h"
#include "./raylib_internal.h"

#include <stddef.h>

#if GLINT_PLATFORM_EMSCRIPTEN
#include <emscripten.h>

static struct {
    gplatform_FrameFn frame;
    gplatform_LoopEndFn end;
    void* user;
} gplatform__loop;

static void gplatform__loop_frame(void* unused);
#endif

bool gplatform_run_loop(gplatform_FrameFn frame, gplatform_LoopEndFn end, void* user) {
    if (frame == NULL) {
        return false;
    }
#if GLINT_PLATFORM_EMSCRIPTEN
    if (gplatform__loop.frame != NULL) {
        return false;
    }

    gplatform__loop.frame = frame;
    gplatform__loop.end = end;
    gplatform__loop.user = user;
    emscripten_set_main_loop_arg(gplatform__loop_frame, NULL, 0, false);
    return true;
#else
    bool ok = true;
    while (!gplatform_window_should_close()) {
        if (!frame(user, gplatform_frame_time())) {
            ok = false;
            break;
        }
    }

    if (end != NULL) {
        end(user, ok);
    }

    return ok;
#endif
}

bool gplatform_loop_running(void) {
#if GLINT_PLATFORM_EMSCRIPTEN
    return gplatform__loop.frame != NULL;
#else
    return false;
#endif
}

void gplatform_cancel_loop(void* user) {
#if GLINT_PLATFORM_EMSCRIPTEN
    if (gplatform__loop.frame != NULL && gplatform__loop.user == user) {
        gplatform__loop.frame = NULL;
        emscripten_cancel_main_loop();
    }
#else
    (void)user;
#endif
}

bool gplatform_window_open(int width, int height, const char* title, bool vsync, bool resizable, int fps) {
    graylib_init_logging();

    unsigned flags = 0;
    if (vsync) {
        flags |= FLAG_VSYNC_HINT;
    }

    if (resizable) {
        flags |= FLAG_WINDOW_RESIZABLE;
    }

    if (flags > 0) {
        SetConfigFlags(flags);
    }

    InitWindow(width, height, title);
    if (!IsWindowReady()) {
        return false;
    }

    SetTargetFPS(fps);
    return true;
}

void gplatform_window_close(void) {
    graylib_init_logging();

    CloseWindow();
}

bool gplatform_window_should_close(void) {
    return WindowShouldClose();
}

float gplatform_frame_time(void) {
    return GetFrameTime();
}

double gplatform_time(void) {
    return GetTime();
}

int gplatform_width(void) {
    return GetScreenWidth();
}

int gplatform_height(void) {
    return GetScreenHeight();
}

#if GLINT_PLATFORM_EMSCRIPTEN
static void gplatform__loop_frame(void* unused) {
    (void)unused;
    bool ok = true;

    ok = gplatform__loop.frame(gplatform__loop.user, gplatform_frame_time());
    if (ok) {
        return;
    }

    gplatform_LoopEndFn end = gplatform__loop.end;
    void* user = gplatform__loop.user;
    gplatform__loop.frame = NULL;
    emscripten_cancel_main_loop();
    if (end != NULL) {
        end(user, ok);
    }
}
#endif

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*gplatform_FrameFn)(void* user, float delta);
typedef void (*gplatform_LoopEndFn)(void* user, bool ok);

bool gplatform_run_loop(gplatform_FrameFn frame, gplatform_LoopEndFn end, void* user);

bool gplatform_loop_running(void);

void gplatform_cancel_loop(void* user);

bool gplatform_window_open(int width, int height, const char* title, bool vsync, bool resizable, int fps);

void gplatform_window_close(void);

bool gplatform_window_should_close(void);

float gplatform_frame_time(void);

double gplatform_time(void);

int gplatform_width(void);

int gplatform_height(void);

#ifdef __cplusplus
}
#endif

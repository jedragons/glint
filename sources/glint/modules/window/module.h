#pragma once

#include <glint/platform/logging.h>
#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef float (*gwindow_FrameTimeFn)(void* user);

/// Set `create_window` to open a window; otherwise installs bindings only.
///
/// # Ownership
///
/// The title is borrowed during load. Clock callback state must outlive the Lua state.
typedef struct gwindow_Host {
    gwindow_FrameTimeFn frame_time;
    void* user;
    bool create_window;
    int width;
    int height;
    int fps;
    const char* title;
    bool vsync;
    bool resizable;
    glog_Level log_level;
} gwindow_Host;

typedef struct gwindow_Service {
    bool open;
    bool owns_lua_context;
    bool owns_time_context;
} gwindow_Service;

#define GWINDOW_MODULE_NAME "glint.window"

gmod_Module gwindow_module(void);

#ifdef __cplusplus
}
#endif

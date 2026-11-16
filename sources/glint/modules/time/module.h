#pragma once

#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef float (*gtime_FrameTimeFn)(void* user);

/// A NULL clock callback supplies zero delta.
///
/// # Ownership
///
/// Callback and user data must outlive the Lua state, including after module unload.
typedef struct gtime_Host {
    gtime_FrameTimeFn frame_time;
    void* user;
} gtime_Host;

gmod_Module gtime_module(void);

#ifdef __cplusplus
}
#endif

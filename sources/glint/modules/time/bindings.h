#pragma once

#include "./module.h"

typedef struct lua_State lua_State;

#ifdef __cplusplus
extern "C" {
#endif

/// Bind Timer, Stopwatch and TimerMode. A NULL clock supplies zero delta.
///
/// # Ownership
///
/// World and callback state must outlive the Lua state; existing values retain their bindings.
///
/// # Errors
///
/// Call under Lua protection to catch allocation errors.
bool gtime_bindings_bind(lua_State* lua, gecs_World* world, gtime_FrameTimeFn frame_time, void* frame_time_user);

/// Install bindings under Lua protection. On failure, leave the error on the stack.
bool gtime_bindings_install(lua_State* lua, gecs_World* world, gtime_FrameTimeFn frame_time, void* user);

#ifdef __cplusplus
}
#endif

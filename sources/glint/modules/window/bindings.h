#pragma once

#include "./module.h"

typedef struct lua_State lua_State;

#ifdef __cplusplus
extern "C" {
#endif

/// Bind `window` globals. A NULL clock uses the platform frame time.
///
/// # Ownership
///
/// The clock callback and user data must outlive the Lua state.
///
/// # Errors
///
/// Call under Lua protection to catch allocation errors.
bool gwindow_bindings_bind(galloc_Allocator alloc, lua_State* lua, gwindow_FrameTimeFn frame_time, void* user);

/// Install bindings under Lua protection. On failure, leave the error on the stack.
bool gwindow_bindings_install(galloc_Allocator alloc, lua_State* lua, gwindow_FrameTimeFn frame_time, void* user);

#ifdef __cplusplus
}
#endif

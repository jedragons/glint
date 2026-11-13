#ifndef GWINDOW_WINDOW_LUA_H_
#define GWINDOW_WINDOW_LUA_H_

#include <glint/alloc.h>

#include <stdbool.h>

typedef struct lua_State lua_State;
typedef float (*gwindow_FrameTimeFn)(void *user);

/* Registers read-only window.dt/time/width/height and log_* globals. The
 * frame-time callback and its user data must outlive the Lua state. A NULL
 * callback uses raylib's GetFrameTime(). Lua allocations use its own state
 * allocator. Call within a protected Lua call to catch allocation errors. */
bool gwindow_bind_lua(galloc_Allocator alloc, lua_State *lua, gwindow_FrameTimeFn frame_time, void *user);

#endif // GWINDOW_WINDOW_LUA_H_

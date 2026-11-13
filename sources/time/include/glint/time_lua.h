#ifndef GTIME_TIME_LUA_H
#define GTIME_TIME_LUA_H

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <glint/modules.h>
#include <stdbool.h>

typedef struct lua_State lua_State;
typedef float (*gtime_FrameTimeFn)(void *user);

/* Together with gtypes_LuaService, supplies the time module's frame clock.
 * The callback and user pointer must outlive the Lua state, since Lua values
 * can retain them after module unload. A NULL callback supplies zero. */
typedef struct gtime_Host {
    gtime_FrameTimeFn frame_time;
    void *user;
} gtime_Host;
gmod_Module gtime_module(void);

/* Register Timer, Stopwatch and TimerMode in Lua and their ECS component types.
 * world, alloc and frame_time_user for every binding must outlive the Lua state,
 * including after rebinding. Existing objects retain their original binding.
 * A NULL frame_time supplies zero for tick() calls without an explicit delta.
 * Use within a protected Lua call if Lua allocation errors need to be caught. */
bool gtime_bind_lua(
    galloc_Allocator alloc,
    lua_State *lua,
    gecs_World *world,
    gtime_FrameTimeFn frame_time,
    void *frame_time_user
);

#endif // GTIME_TIME_LUA_H

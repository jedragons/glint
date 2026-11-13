#include <glint/time_lua.h>
#include <glint/types_lua.h>
#include <lauxlib.h>
#include <lua.h>

typedef struct gtime__TimeBindArgs {
    galloc_Allocator alloc;
    gecs_World *world;
    gtime_Host host;
} gtime__TimeBindArgs;

static int gtime__module_time_bind(lua_State *lua) {
    gtime__TimeBindArgs *args = lua_touserdata(lua, 1);
    if (!gtime_bind_lua(args->alloc, lua, args->world, args->host.frame_time, args->host.user))
        return luaL_error(lua, "could not register time components");
    return 0;
}

static bool gtime__module_time_load(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gecs_Component lua_type = GECS_CONTEXT(alloc, world, gtypes_LuaService);
    gecs_Component host_type = GECS_CONTEXT(alloc, world, gtime_Host);
    gtypes_LuaService *lua_service = lua_type ? gecs_context_get(world, lua_type) : NULL;
    gtime_Host *host = host_type ? gecs_context_get(world, host_type) : NULL;
    if (!lua_service || !lua_service->lua || !host) return false;
    gtime__TimeBindArgs args = {.alloc = alloc, .world = world, .host = *host};
    lua_pushcfunction(lua_service->lua, gtime__module_time_bind);
    lua_pushlightuserdata(lua_service->lua, &args);
    return lua_pcall(lua_service->lua, 1, 0, 0) == LUA_OK;
}

gmod_Module gtime_module(void) {
    return (gmod_Module){GMOD_INIT, .name = "glint.time", .load = gtime__module_time_load};
}

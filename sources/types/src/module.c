#include <glint/types_lua.h>
#include <lauxlib.h>
#include <lua.h>

static int gtypes__module_types_bind(lua_State *lua) {
    galloc_Allocator *alloc = lua_touserdata(lua, 1);
    if (!gtypes_bind_lua(*alloc, lua)) return luaL_error(lua, "could not register type bindings");
    return 0;
}

static bool gtypes__module_types_load(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gecs_Component type = GECS_CONTEXT(alloc, world, gtypes_LuaService);
    gtypes_LuaService *service = type ? gecs_context_get(world, type) : NULL;
    if (!service || !service->lua) return false;
    lua_pushcfunction(service->lua, gtypes__module_types_bind);
    lua_pushlightuserdata(service->lua, &alloc);
    return lua_pcall(service->lua, 1, 0, 0) == LUA_OK;
}

gmod_Module gtypes_module(void) {
    return (gmod_Module){GMOD_INIT, .name = "glint.types", .load = gtypes__module_types_load};
}

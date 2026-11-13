#include <glint/dim2/dim2.h>
#include <glint/types_lua.h>
#include <lauxlib.h>
#include <lua.h>

typedef struct gdim2__BindArgs {
    galloc_Allocator alloc;
    gdim2_Dim2 *dim2;
} gdim2__BindArgs;

static bool gdim2__module_dim2_import(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    (void)world;
    (void)user;
    gmod_Module types = gtypes_module();
    return gmod_Modules_import(alloc, modules, &types);
}

static int gdim2__module_bind_protected(lua_State *lua) {
    gdim2__BindArgs *args = lua_touserdata(lua, 1);
    if (!gdim2_bind_lua(args->alloc, args->dim2, lua))
        return luaL_error(lua, "could not register 2D bindings");
    return 0;
}

static bool gdim2__module_dim2_load(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gecs_Component host_type = GECS_CONTEXT(alloc, world, gdim2_Host);
    gecs_Component service_type = GECS_CONTEXT(alloc, world, gdim2_Service);
    gdim2_Host *host = gecs_context_get(world, host_type);
    gecs_Component lua_type = GECS_CONTEXT(alloc, world, gtypes_LuaService);
    gtypes_LuaService *lua_service = lua_type ? gecs_context_get(world, lua_type) : NULL;
    if (!host_type || !service_type || !host || !host->vfs || !host->script ||
        !lua_service || lua_service->lua != gscript_state(host->script) ||
        gecs_context_get(world, service_type)) return false;
    gdim2_Service service = {.delta_time = host->delta_time, .defer_destroy = host->defer_destroy};
    if (host->enabled) {
        service.instance = gdim2_new(alloc, world, host->vfs);
        if (!service.instance) return false;
        lua_State *lua = gscript_state(host->script);
        gdim2__BindArgs args = {.alloc = alloc, .dim2 = service.instance};
        lua_pushcfunction(lua, gdim2__module_bind_protected);
        lua_pushlightuserdata(lua, &args);
        if (lua_pcall(lua, 1, 0, 0) != LUA_OK) {
            gdim2_destroy(alloc, service.instance);
            return false;
        }
    }
    if (!gecs_context_set(alloc, world, service_type, &service)) {
        gdim2_destroy(alloc, service.instance);
        return false;
    }
    return true;
}

static gdim2_Service *gdim2__module_dim2_service(galloc_Allocator alloc, gecs_World *world) {
    gecs_Component type = GECS_CONTEXT(alloc, world, gdim2_Service);
    return type ? gecs_context_get(world, type) : NULL;
}

static void gdim2__module_dim2_update(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gdim2_Service *service = gdim2__module_dim2_service(alloc, world);
    if (service && service->instance)
        gdim2_update(alloc, service->instance, service->delta_time ? *service->delta_time : 0.0f);
}

static void gdim2__module_dim2_render(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gdim2_Service *service = gdim2__module_dim2_service(alloc, world);
    if (service && service->instance) gdim2_render(service->instance);
}

static void gdim2__module_dim2_unload(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gecs_Component type = GECS_CONTEXT(alloc, world, gdim2_Service);
    gdim2_Service *service = type ? gecs_context_get(world, type) : NULL;
    if (!service) return;
    if (!service->defer_destroy) gdim2_destroy(alloc, service->instance);
    gecs_context_remove(alloc, world, type);
}

gmod_Module gdim2_module(void) {
    return (gmod_Module){GMOD_INIT, .name = GDIM2_MODULE_NAME,
                         .import = gdim2__module_dim2_import, .load = gdim2__module_dim2_load, .unload = gdim2__module_dim2_unload,
                         .update = gdim2__module_dim2_update, .render = gdim2__module_dim2_render};
}

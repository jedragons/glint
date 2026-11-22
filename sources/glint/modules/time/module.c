#include "./module.h"
#include "./bindings.h"
#include <glint/modules/types/bindings.h>
#include <glint/modules/types/module.h>
#include <glint/modules/window/module.h>

static bool gtime__module_time_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gtime__module_time_load(gecs_World* world, void* user, char* error, size_t error_size);

gmod_Module gtime_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = "glint.time",
        .import = gtime__module_time_import,
        .load = gtime__module_time_load,
    };
}

static bool gtime__module_time_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component lua = gecs_context_find(world, "gtypes_LuaService");
    gecs_Component clock = gecs_context_find(world, "gtime_Host");
    bool lua_available = lua > 0 && gecs_context_get(world, lua) != NULL;
    bool clock_available = clock > 0 && gecs_context_get(world, clock) != NULL;
    if (lua_available && clock_available) {
        return true;
    }

    return gmod_Modules_import(alloc, modules, gwindow_module());
}

static bool gtime__module_time_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    gecs_Component lua_type = GECS_CONTEXT(world, gtypes_LuaService);
    gecs_Component host_type = GECS_CONTEXT(world, gtime_Host);
    gtypes_LuaService* lua_service = lua_type != 0 ? gecs_context_get(world, lua_type) : NULL;
    gtime_Host* host = host_type != 0 ? gecs_context_get(world, host_type) : NULL;
    if (lua_service == NULL || lua_service->lua == NULL || host == NULL) {
        return false;
    }

    return gtime_bindings_install(lua_service->lua, world, host->frame_time, host->user);
}

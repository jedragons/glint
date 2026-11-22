#include "./module.h"

#include "./bindings.h"

#include <glint/modules/names/names.h>
#include <glint/modules/script/module.h>
#include <glint/modules/texture/texture_asset.h>
#include <glint/modules/time/module.h>
#include <glint/modules/time/time.h>
#include <glint/modules/types/bindings.h>
#include <glint/platform/windowing.h>
#include <glint/runtime/config.h>

static float gwindow__frame_time(void* user);
static bool gwindow__import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gwindow__load(gecs_World* world, void* user, char* error, size_t error_size);
static void gwindow__unload(gecs_World* world, void* user);

gmod_Module gwindow_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = GWINDOW_MODULE_NAME,
        .import = gwindow__import,
        .load = gwindow__load,
        .unload = gwindow__unload,
    };
}

static float gwindow__frame_time(void* user) {
    return ((const gtime_FrameTime*)user)->delta;
}

static bool gwindow__import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)world;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    return gmod_Modules_import(alloc, modules, gscript_module());
}

static bool gwindow__load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component script_type = GECS_CONTEXT(world, gscript_Service);
    gecs_Component lua_type = GECS_CONTEXT(world, gtypes_LuaService);
    gecs_Component time_type = GECS_CONTEXT(world, gtime_Host);
    gecs_Component host_type = GECS_CONTEXT(world, gwindow_Host);
    gecs_Component service_type = GECS_CONTEXT(world, gwindow_Service);
    if (service_type == 0 || gecs_context_get(world, service_type) != NULL) {
        return false;
    }

    bool contexts_registered = script_type != 0 && lua_type != 0 && time_type != 0 && host_type != 0;
    if (!contexts_registered) {
        return false;
    }

    gscript_Service* script = gecs_context_get(world, script_type);
    if (script == NULL || script->script == NULL) {
        return false;
    }

    lua_State* lua = gscript_lua_state(script->script);
    gtypes_LuaService* existing = gecs_context_get(world, lua_type);
    if (existing != NULL && existing->lua != lua) {
        return false;
    }

    const gwindow_Host* host = gecs_context_get(world, host_type);
    gwindow_Host configured;
    gecs_Component config_type = gecs_context_find(world, "glint_Config");
    const glint_Config* config = config_type != 0 ? gecs_context_get(world, config_type) : NULL;
    if (host == NULL && config != NULL) {
        gecs_Component clock_type = gecs_context_find(world, "gtime_FrameTime");
        gtime_FrameTime* clock = clock_type != 0 ? gecs_context_get(world, clock_type) : NULL;
        configured = (gwindow_Host) {
            .frame_time = clock != NULL ? gwindow__frame_time : NULL,
            .user = clock,
            .create_window = !config->headless,
            .width = config->width,
            .height = config->height,
            .title = config->title,
            .vsync = config->vsync,
            .resizable = config->resizable,
            .fps = config->fps,
            .log_level = config->log_level
        };
        host = &configured;
    }

    gwindow_Service state = {
        .owns_lua_context = existing == NULL,
        .owns_time_context = gecs_context_get(world, time_type) == NULL,
    };

    if (host != NULL && host->log_level != GLOG_DEFAULT) {
        glog_set_level(host->log_level);
    }

    if (host != NULL && host->create_window) {
        if (!gplatform_window_open(
                host->width > 0 ? host->width : 800,
                host->height > 0 ? host->height : 600,
                host->title != NULL ? host->title : "glint",
                host->vsync,
                host->resizable,
                host->fps > 0 ? host->fps : 0
            )) {
            return false;
        }

        state.open = true;
    }

    gwindow_Host binding_host = host != NULL ? *host : (gwindow_Host) {0};
    if (!gwindow_bindings_install(alloc, lua, binding_host.frame_time, binding_host.user)) {
        goto error;
    }

    gtypes_LuaService lua_service = {.lua = lua};
    gtime_Host time_host = {.frame_time = binding_host.frame_time, .user = binding_host.user};
    if (state.owns_lua_context && gecs_context_set(world, lua_type, &lua_service) == NULL) {
        goto error;
    }

    if (state.owns_time_context && gecs_context_set(world, time_type, &time_host) == NULL) {
        goto error;
    }

    if (gecs_context_set(world, service_type, &state) == NULL) {
        goto error;
    }

    return true;

error:
    gecs_context_remove(world, service_type);
    if (state.open) {
        gplatform_window_close();
    }

    if (state.owns_lua_context) {
        gecs_context_remove(world, lua_type);
    }

    if (state.owns_time_context) {
        gecs_context_remove(world, time_type);
    }

    return false;
}

static void gwindow__unload(gecs_World* world, void* user) {
    (void)user;

    gecs_Component service_type = gecs_context_find(world, "gwindow_Service");
    gwindow_Service* state = service_type != 0 ? gecs_context_get(world, service_type) : NULL;
    if (state == NULL) {
        return;
    }

    if (state->open) {
        gplatform_window_close();
    }

    if (state->owns_lua_context) {
        gecs_context_remove(world, gecs_context_find(world, "gtypes_LuaService"));
    }

    if (state->owns_time_context) {
        gecs_context_remove(world, gecs_context_find(world, "gtime_Host"));
    }

    gecs_context_remove(world, service_type);
}

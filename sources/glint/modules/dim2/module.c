#include "./module.h"
#include "./bindings.h"

#include <glint/modules/assets/module.h>
#include <glint/modules/script/module.h>
#include <glint/modules/texture/module.h>
#include <glint/modules/time/time.h>
#include <glint/modules/types/bindings.h>
#include <glint/modules/types/module.h>
#include <glint/modules/vfs/module.h>
#include <glint/modules/window/module.h>
#include <glint/runtime/config.h>

static bool gdim2__module_dim2_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gdim2__module_dim2_load(gecs_World* world, void* user, char* error, size_t error_size);
static gdim2_Service* gdim2__module_dim2_service(gecs_World* world);
static bool gdim2__module_dim2_update(gecs_World* world, void* user, char* error, size_t error_size);
static bool gdim2__module_dim2_render(gecs_World* world, void* user, char* error, size_t error_size);
static bool gdim2__module_dim2_can_unload(gecs_World* world, void* user);
static void gdim2__module_dim2_unload(gecs_World* world, void* user);

gmod_Module gdim2_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = GDIM2_MODULE_NAME,
        .import = gdim2__module_dim2_import,
        .load = gdim2__module_dim2_load,
        .unload = gdim2__module_dim2_unload,
        .can_unload = gdim2__module_dim2_can_unload,
        .update = gdim2__module_dim2_update,
        .render = gdim2__module_dim2_render,
        .update_order = 1,
        .render_order = -1,
    };
}

static bool gdim2__module_dim2_import(gmod_Modules* modules, gecs_World* world, void* user) {
    galloc_Allocator alloc = glint_allocator(world);
    (void)world;
    (void)user;
    if (!gmod_Modules_import(alloc, modules, gwindow_module())) {
        return false;
    }

    if (!gmod_Modules_import(alloc, modules, gtexture_module())) {
        return false;
    }

    return gmod_Modules_import(alloc, modules, gtypes_module());
}

static bool gdim2__module_dim2_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component host_type = GECS_CONTEXT(world, gdim2_Host);
    gecs_Component service_type = GECS_CONTEXT(world, gdim2_Service);
    const gdim2_Host* host = gecs_context_get(world, host_type);
    gdim2_Host configured;
    gecs_Component config_type = gecs_context_find(world, "glint_Config");
    const glint_Config* config = config_type != 0 ? gecs_context_get(world, config_type) : NULL;

    if (host == NULL && config != NULL) {
        gecs_Component vfs_type = gecs_context_find(world, "gvfsmod_Service");
        gecs_Component asset_type = gecs_context_find(world, "gasset_Service");
        gecs_Component script_type = gecs_context_find(world, "gscript_Service");
        gecs_Component clock_type = gecs_context_find(world, "gtime_FrameTime");

        const gvfsmod_Service* vfs = vfs_type != 0 ? gecs_context_get(world, vfs_type) : NULL;
        const gasset_Service* assets = asset_type != 0 ? gecs_context_get(world, asset_type) : NULL;
        const gscript_Service* script = script_type != 0 ? gecs_context_get(world, script_type) : NULL;
        const gtime_FrameTime* clock = clock_type != 0 ? gecs_context_get(world, clock_type) : NULL;

        configured = (gdim2_Host) {
            .vfs = vfs != NULL ? vfs->vfs : NULL,
            .assets = assets != NULL ? assets->cache : NULL,
            .script = script != NULL ? script->script : NULL,
            .delta_time = clock != NULL ? &clock->delta : NULL,
            .enabled = !config->headless
        };
        host = &configured;
    }

    gecs_Component lua_type = GECS_CONTEXT(world, gtypes_LuaService);
    gtypes_LuaService* lua_service = lua_type != 0 ? gecs_context_get(world, lua_type) : NULL;

    bool contexts_registered = host_type != 0 && service_type != 0;
    bool host_ready = host != NULL && host->vfs != NULL && host->script != NULL;
    if (!contexts_registered || !host_ready || lua_service == NULL) {
        return false;
    }

    bool lua_matches = lua_service->lua == gscript_lua_state(host->script);
    bool service_exists = gecs_context_get(world, service_type) != NULL;
    if (!lua_matches || service_exists) {
        return false;
    }

    gdim2_Service service = {.delta_time = host->delta_time};
    if (host->enabled) {
        gasset_Cache* cache = host->assets;
        if (cache == NULL) {
            gecs_Component assets_type = GECS_CONTEXT(world, gasset_Service);
            gasset_Service* assets = assets_type != 0 ? gecs_context_get(world, assets_type) : NULL;
            cache = assets != NULL ? assets->cache : NULL;
        }

        service.instance = gdim2_new(world, host->vfs, cache);
        if (service.instance == NULL) {
            return false;
        }

        lua_State* lua = gscript_lua_state(host->script);
        if (!gdim2_bindings_install(alloc, service.instance, lua)) {
            gdim2_destroy(alloc, service.instance);
            return false;
        }
    }

    if (gecs_context_set(world, service_type, &service) == NULL) {
        gdim2_destroy(alloc, service.instance);
        return false;
    }

    return true;
}

static gdim2_Service* gdim2__module_dim2_service(gecs_World* world) {
    gecs_Component type = GECS_CONTEXT(world, gdim2_Service);
    return type != 0 ? gecs_context_get(world, type) : NULL;
}

static bool gdim2__module_dim2_update(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    galloc_Allocator alloc = glint_allocator(world);
    (void)user;
    gdim2_Service* service = gdim2__module_dim2_service(world);
    if (service != NULL && service->instance != NULL) {
        gdim2_update(alloc, service->instance, service->delta_time != NULL ? *service->delta_time : 0.0f);
    }

    return true;
}

static bool gdim2__module_dim2_render(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;
    gdim2_Service* service = gdim2__module_dim2_service(world);
    if (service != NULL && service->instance != NULL) {
        gdim2_render(service->instance);
    }

    return true;
}

static bool gdim2__module_dim2_can_unload(gecs_World* world, void* user) {
    (void)user;
    gecs_Component type = gecs_context_find(world, "gdim2_Service");
    gdim2_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    return service == NULL || !gdim2_busy(service->instance);
}

static void gdim2__module_dim2_unload(gecs_World* world, void* user) {
    galloc_Allocator alloc = glint_allocator(world);
    (void)user;
    gecs_Component type = gecs_context_find(world, "gdim2_Service");
    gdim2_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service == NULL) {
        return;
    }

    if (!gdim2_destroy(alloc, service->instance)) {
        return;
    }

    gecs_context_remove(world, type);
}

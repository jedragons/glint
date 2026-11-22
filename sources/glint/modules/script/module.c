#include "./bindings.h"
#include "script_internal.h"

#include <glint/modules/assets/module.h>
#include <glint/modules/script/module.h>
#include <glint/modules/vfs/module.h>
#include <glint/runtime/config.h>

static bool gscript__module_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gscript__module_load(gecs_World* world, void* user, char* error, size_t error_size);
static bool gscript__module_start(gecs_World* world, void* user, char* error, size_t error_size);
static bool gscript__module_update(gecs_World* world, void* user, char* error, size_t error_size);
static void gscript__module_unload(gecs_World* world, void* user);

gmod_Module gscript_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = GSCRIPT_MODULE_NAME,
        .import = gscript__module_import,
        .load = gscript__module_load,
        .start = gscript__module_start,
        .unload = gscript__module_unload,
        .update = gscript__module_update,
    };
}

static bool gscript__module_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)world;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    return gmod_Modules_import(alloc, modules, gasset_module());
}

static bool gscript__module_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = GECS_CONTEXT(world, gscript_Service);
    gecs_Component vfs_type = GECS_CONTEXT(world, gvfsmod_Service);
    gecs_Component asset_type = GECS_CONTEXT(world, gasset_Service);
    if (type == 0 || vfs_type == 0 || asset_type == 0) {
        return false;
    }

    gscript_Service* existing = gecs_context_get(world, type);
    gvfsmod_Service* vfs = gecs_context_get(world, vfs_type);
    gasset_Service* assets = gecs_context_get(world, asset_type);
    bool vfs_ready = vfs != NULL && vfs->vfs != NULL;
    bool assets_ready = assets != NULL && assets->cache != NULL;
    if (!vfs_ready || !assets_ready) {
        return false;
    }

    if (existing != NULL) {
        return existing->script != NULL && existing->behaviors != NULL;
    }

    gscript_Service service = {.module_owned = true};
    service.script = gscript_lua_new(alloc);
    if (service.script == NULL) {
        return false;
    }

    if (!gscript_bindings_bind_world(service.script, world)) {
        gscript_lua_destroy(alloc, service.script);
        return false;
    }

    service.behaviors = gscript_behavior_new(service.script, world, vfs->vfs, assets->cache);
    if (service.behaviors == NULL) {
        gscript_lua_destroy(alloc, service.script);
        return false;
    }

    if (gecs_context_set(world, type, &service) == NULL) {
        gscript_behavior_destroy(alloc, service.behaviors);
        gscript_lua_destroy(alloc, service.script);
        return false;
    }

    return true;
}

static bool gscript__module_start(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)user;

    gecs_Component config_type = gecs_context_find(world, "glint_Config");
    const glint_Config* config = config_type != 0 ? gecs_context_get(world, config_type) : NULL;
    if (config == NULL || config->initial_script == NULL) {
        return true;
    }

    gecs_Component type = gecs_context_find(world, "gscript_Service");
    gscript_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service == NULL || service->behaviors == NULL) {
        return false;
    }

    gecs_Entity entity = gecs_spawn(world);
    if (entity.id == 0) {
        return false;
    }

    if (gscript_behavior_attach(service->behaviors, entity, config->initial_script)) {
        return true;
    }

    gscript_lua_take_error(service->script, error, error_size);
    gecs_Entity_destroy(entity);
    return false;
}

static bool gscript__module_update(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)user;

    gecs_Component type = GECS_CONTEXT(world, gscript_Service);
    gscript_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service == NULL || !service->module_owned || service->host_updates) {
        return true;
    }

    if (!gscript_behavior__update(glint_allocator(world), service->behaviors, false)) {
        gscript_lua_take_error(service->script, error, error_size);
        return false;
    }

    return true;
}

static void gscript__module_unload(gecs_World* world, void* user) {
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = gecs_context_find(world, "gscript_Service");
    gscript_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service == NULL) {
        return;
    }

    if (service->module_owned && !gscript_behavior_destroy(alloc, service->behaviors)) {
        return;
    }

    if (service->module_owned) {
        gscript_lua_destroy(alloc, service->script);
        gecs_context_remove(world, type);
    }
}

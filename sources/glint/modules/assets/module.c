#include "./module.h"

#include <glint/modules/vfs/module.h>

static bool gasset__module_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gasset__module_load(gecs_World* world, void* user, char* error, size_t error_size);
static bool gasset__module_update(gecs_World* world, void* user, char* error, size_t error_size);
static bool gasset__module_can_unload(gecs_World* world, void* user);
static void gasset__module_unload(gecs_World* world, void* user);

gmod_Module gasset_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = GASSET_MODULE_NAME,
        .import = gasset__module_import,
        .load = gasset__module_load,
        .unload = gasset__module_unload,
        .can_unload = gasset__module_can_unload,
        .update = gasset__module_update,
    };
}

static bool gasset__module_import(gmod_Modules* modules, gecs_World* world, void* user) {
    galloc_Allocator alloc = glint_allocator(world);
    (void)world;
    (void)user;
    return gmod_Modules_import(alloc, modules, gvfsmod_module());
}

static bool gasset__module_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    galloc_Allocator alloc = glint_allocator(world);
    (void)user;
    gecs_Component type = GECS_CONTEXT(world, gasset_Service);
    if (type == 0) {
        return false;
    }

    gasset_Service* existing = gecs_context_get(world, type);
    if (existing != NULL) {
        return existing->cache != NULL;
    }

    gasset_Service service = {
        .cache = gasset_Cache_create(alloc),
        .module_owned = true,
    };

    if (service.cache == NULL) {
        return false;
    }

    if (gecs_context_set(world, type, &service) == NULL) {
        gasset_Cache_destroy(alloc, service.cache);
        return false;
    }

    return true;
}

static bool gasset__module_update(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = GECS_CONTEXT(world, gasset_Service);
    gasset_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;

    if (service != NULL && service->module_owned && !service->host_polls) {
        gasset_Cache_poll(alloc, service->cache);
    }

    return true;
}

static bool gasset__module_can_unload(gecs_World* world, void* user) {
    (void)user;
    gecs_Component type = gecs_context_find(world, "gasset_Service");
    gasset_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    return service == NULL || !gasset_Cache_busy(service->cache);
}

static void gasset__module_unload(gecs_World* world, void* user) {
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = gecs_context_find(world, "gasset_Service");
    gasset_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;

    if (service != NULL && service->module_owned) {
        gasset_Cache_destroy(alloc, service->cache);
        gecs_context_remove(world, type);
    }
}

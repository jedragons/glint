#include "./module.h"

#include <glint/modules/assets/module.h>
#include <glint/modules/vfs/module.h>

static bool gtexture__module_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gtexture__module_load(gecs_World* world, void* user, char* error, size_t error_size);
static void gtexture__module_unload(gecs_World* world, void* user);

gmod_Module gtexture_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = GTEXTURE_MODULE_NAME,
        .import = gtexture__module_import,
        .load = gtexture__module_load,
        .unload = gtexture__module_unload,
    };
}

static bool gtexture__module_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)world;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    return gmod_Modules_import(alloc, modules, gasset_module());
}

static bool gtexture__module_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component st = GECS_CONTEXT(world, gtexture_Service);
    gecs_Component at = GECS_CONTEXT(world, gasset_Service);
    gecs_Component vt = GECS_CONTEXT(world, gvfsmod_Service);
    if (st == 0 || at == 0 || vt == 0) {
        return false;
    }

    gtexture_Service* existing = gecs_context_get(world, st);
    gasset_Service* cache = gecs_context_get(world, at);
    gvfsmod_Service* vfs = gecs_context_get(world, vt);
    if (cache == NULL || vfs == NULL) {
        return false;
    }

    if (existing != NULL) {
        return gtexture_matches(existing->assets, cache->cache, vfs->vfs);
    }

    gtexture_Service service = {.assets = gtexture_new(alloc, cache->cache, vfs->vfs), .module_owned = true};
    if (service.assets == NULL) {
        return false;
    }

    if (gecs_context_set(world, st, &service) == NULL) {
        gtexture_destroy(alloc, service.assets);
        return false;
    }

    return true;
}

static void gtexture__module_unload(gecs_World* world, void* user) {
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = gecs_context_find(world, "gtexture_Service");
    gtexture_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service == NULL || !service->module_owned) {
        return;
    }

    if (!gtexture_destroy(alloc, service->assets)) {
        return;
    }

    gecs_context_remove(world, type);
}

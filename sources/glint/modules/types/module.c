#include "./module.h"
#include "./bindings.h"
#include <glint/modules/window/module.h>

static bool gtypes__module_types_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gtypes__module_types_load(gecs_World* world, void* user, char* error, size_t error_size);

gmod_Module gtypes_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = "glint.types",
        .import = gtypes__module_types_import,
        .load = gtypes__module_types_load,
    };
}

static bool gtypes__module_types_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = gecs_context_find(world, "gtypes_LuaService");
    if (type > 0 && gecs_context_get(world, type) != NULL) {
        return true;
    }

    return gmod_Modules_import(alloc, modules, gwindow_module());
}

static bool gtypes__module_types_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = GECS_CONTEXT(world, gtypes_LuaService);
    gtypes_LuaService* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service == NULL || service->lua == NULL) {
        return false;
    }

    return gtypes_bindings_install(alloc, service->lua);
}

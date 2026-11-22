// Standalone shared-library fixture; see shared_module_host.c.
#include <glint/runtime/modules.h>

static void gmod__test_destroy_value(void* value);
static bool gmod__test_import_module(gmod_Modules* modules, gecs_World* world, void* user);

GMOD_EXPORT const gmod_Module* gmod_descriptor(void) {
    static const gmod_Module gmod__test_module = {GMOD_INIT, .name = "test.dll", .import = gmod__test_import_module};
    return &gmod__test_module;
}

static void gmod__test_destroy_value(void* value) {
    int** counter = value;
    **counter += 1;
}

static bool gmod__test_import_module(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)modules;
    gecs_Component component =
        gecs_Component_register(world, "test.dll.counter.v1", sizeof(int*), gmod__test_destroy_value);
    gecs_Entity entity = gecs_spawn(world);
    int* counter = user;
    return component != 0 && entity.id != 0 && gecs_Component_set(entity, component, &counter) != NULL;
}

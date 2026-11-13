/* Standalone shared-library fixture; see shared_module_host.c. */
#include <glint/modules.h>

static void gmod__test_destroy_value(void *value) {
    int **counter = value;
    ++**counter;
}

static bool gmod__test_import_module(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    (void)modules;
    gecs_Component id = gecs_Component_register(alloc, world, "test.dll.counter.v1", sizeof(int *), gmod__test_destroy_value);
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    int *counter = user;
    return id && entity && gecs_Component_set(alloc, world, entity, id, &counter);
}

GMOD_EXPORT const gmod_Module *gmod_descriptor(void) {
    static const gmod_Module gmod__test_module = {GMOD_INIT, .name = "test.dll", .import = gmod__test_import_module};
    return &gmod__test_module;
}

#include "./module.h"

static bool grender__load(gecs_World* world, void* user, char* error, size_t error_size);
static void grender__unload(gecs_World* world, void* user);

gmod_Module grender_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = "glint.render",
        .load = grender__load,
        .unload = grender__unload,
    };
}

static bool grender__load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    gecs_Component type = GECS_CONTEXT(world, grender_Service);
    if (type == 0) {
        return false;
    }

    grender_Service* existing = gecs_context_get(world, type);
    if (existing != NULL) {
        return existing->draw_texture != NULL && existing->clear_background != NULL;
    }

    grender_Service service = {
        .draw_texture = grender_draw_texture_params,
        .clear_background = grender_clear_background,
        .module_owned = true
    };

    return gecs_context_set(world, type, &service) != NULL;
}

static void grender__unload(gecs_World* world, void* user) {
    (void)user;

    gecs_Component type = gecs_context_find(world, "grender_Service");
    grender_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service != NULL && service->module_owned) {
        gecs_context_remove(world, type);
    }
}

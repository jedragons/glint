#include <glint/audio.h>

static bool gaudio__module_audio_load(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gecs_Component type = GECS_CONTEXT(alloc, world, gaudio_Service);
    if (!type) return false;
    if (gecs_context_get(world, type)) return true;

    gaudio_Service service = {.module_owned = true};
    if (!gaudio_Device_open(alloc, &service.device, 0)) return false;
    if (!gecs_context_set(alloc, world, type, &service)) {
        gaudio_Device_close(&service.device);
        return false;
    }
    return true;
}

static void gaudio__module_audio_unload(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)user;
    gecs_Component type = GECS_CONTEXT(alloc, world, gaudio_Service);
    gaudio_Service *service = type ? gecs_context_get(world, type) : NULL;
    if (service && service->module_owned) {
        gaudio_Device_close(&service->device);
        gecs_context_remove(alloc, world, type);
    }
}

gmod_Module gaudio_module(void) {
    return (gmod_Module) {GMOD_INIT, .name = "glint.audio",
                          .load = gaudio__module_audio_load, .unload = gaudio__module_audio_unload};
}

#include <glint/modules/audio/module.h>
#include <glint/runtime/config.h>

static bool gaudio__module_audio_load(gecs_World* world, void* user, char* error, size_t error_size);
static void gaudio__module_audio_unload(gecs_World* world, void* user);

gmod_Module gaudio_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = "glint.audio",
        .load = gaudio__module_audio_load,
        .unload = gaudio__module_audio_unload,
    };
}

static bool gaudio__module_audio_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    galloc_Allocator alloc = glint_allocator(world);
    (void)user;
    gecs_Component type = GECS_CONTEXT(world, gaudio_Service);
    if (type == 0) {
        return false;
    }

    if (gecs_context_get(world, type) != NULL) {
        return true;
    }

    gaudio_Service service = {.module_owned = true};
    gecs_Component host_type = GECS_CONTEXT(world, gmod_Host);
    if (host_type == 0) {
        return false;
    }

    const gmod_Host* host = gecs_context_get(world, host_type);
    gecs_Component config_type = gecs_context_find(world, "glint_Config");
    const glint_Config* config = config_type != 0 ? gecs_context_get(world, config_type) : NULL;
    bool headless = host != NULL ? host->headless : config != NULL && config->headless;
    bool allow_unavailable = host != NULL ? host->allow_unavailable_devices : config != NULL;
    if (!headless) {
        bool device_opened = gaudio_Device_open(alloc, &service.device, 0);
        if (!device_opened && !allow_unavailable) {
            return false;
        }
    }

    if (gecs_context_set(world, type, &service) == NULL) {
        gaudio_Device_close(&service.device);
        return false;
    }

    return true;
}

static void gaudio__module_audio_unload(gecs_World* world, void* user) {
    (void)user;
    gecs_Component type = gecs_context_find(world, "gaudio_Service");
    gaudio_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service != NULL && service->module_owned) {
        gaudio_Device_close(&service->device);
        gecs_context_remove(world, type);
    }
}

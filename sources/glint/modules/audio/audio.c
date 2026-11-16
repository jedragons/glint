#include "./audio.h"

#include <glint/platform/audio.h>

#include <limits.h>

bool gaudio_Device_open(galloc_Allocator alloc, gaudio_Device* device, uint32_t buffer_size) {
    (void)alloc;

    if (device == NULL || device->open || buffer_size > INT_MAX) {
        return false;
    }

    if (gplatform_audio_ready()) {
        *device = (gaudio_Device) {.open = true};
        return true;
    }

    gplatform_audio_open(buffer_size != 0 ? (int)buffer_size : 2048);
    if (!gplatform_audio_ready()) {
        return false;
    }
    *device = (gaudio_Device) {.open = true, .owns_device = true};
    return true;
}

void gaudio_Device_close(gaudio_Device* device) {
    if (device == NULL) {
        return;
    }

    if (device->open && device->owns_device && gplatform_audio_ready()) {
        gplatform_audio_close();
    }
    *device = (gaudio_Device) {0};
}

bool gaudio_Device_ready(const gaudio_Device* device) {
    return device != NULL && device->open && gplatform_audio_ready();
}

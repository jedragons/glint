#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Zero-initialized handles can open or borrow the audio device.
/// Borrowed handles do not close it; its owner must outlive them.
typedef struct gaudio_Device {
    bool open;
    bool owns_device;
} gaudio_Device;

/// Open or borrow a device. `buffer_size` is in sample frames; zero uses 2048.
/// Only a newly opened device takes the requested size.
bool gaudio_Device_open(galloc_Allocator alloc, gaudio_Device* device, uint32_t buffer_size);

/// Close the audio device if this handle owns it
void gaudio_Device_close(gaudio_Device* device);

bool gaudio_Device_ready(const gaudio_Device* device);

#ifdef __cplusplus
}
#endif

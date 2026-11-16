#pragma once

#include <glint/modules/audio/audio.h>
#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Borrowed world service; do not close its device directly.
typedef struct gaudio_Service {
    gaudio_Device device;
    bool module_owned;
} gaudio_Service;

/// Own a device until unload, or borrow a host-supplied service.
/// `gmod_Host` controls headless operation and unavailable-device handling.
gmod_Module gaudio_module(void);

#ifdef __cplusplus
}
#endif

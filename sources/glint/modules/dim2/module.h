#pragma once

#include <glint/modules/dim2/dim2.h>
#include <glint/modules/script/script.h>
#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Optional host settings, supplied before module load.
///
/// # Ownership
///
/// Pointers must outlive the loaded module. A disabled host creates an empty service.
typedef struct gdim2_Host {
    gvfs_Vfs* vfs;
    gasset_Cache* assets; // NULL creates a private cache unless a world service exists.
    gscript_lua_Runtime* script;
    const float* delta_time;
    bool enabled;
} gdim2_Host;

typedef struct gdim2_Service {
    gdim2_Dim2* instance;
    const float* delta_time;
} gdim2_Service;

#define GDIM2_MODULE_NAME "glint.dim2"

/// Use `gdim2_Host` if supplied, otherwise the game's services and settings.
gmod_Module gdim2_module(void);

#ifdef __cplusplus
}
#endif

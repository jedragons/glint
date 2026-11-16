#pragma once

#include <glint/runtime/modules.h>
#include <glint/vfs/vfs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GVFSMOD_MODULE_NAME "glint.vfs"

typedef struct gvfsmod_Mount {
    const char* point;
    const char* root;
} gvfsmod_Mount;

/// Mount `asset_root` at /, then additional mounts. NULL uses the working directory.
/// Pointers are borrowed during load.
typedef struct gvfsmod_Host {
    const char* asset_root;
    const gvfsmod_Mount* mounts;
    size_t mount_count;
} gvfsmod_Host;

/// Host-supplied services have `module_owned = false` and must outlive the module.
/// Do not replace the service while loaded.
typedef struct gvfsmod_Service {
    gvfs_Vfs* vfs;
    bool module_owned;
} gvfsmod_Service;

/// Create and mount a VFS unless the host supplies a service.
gmod_Module gvfsmod_module(void);

#ifdef __cplusplus
}
#endif

#include <glint/modules/vfs/module.h>
#include <glint/runtime/config.h>

static bool gvfsmod__module_load(gecs_World* world, void* user, char* error, size_t error_size);
static void gvfsmod__module_unload(gecs_World* world, void* user);

gmod_Module gvfsmod_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = GVFSMOD_MODULE_NAME,
        .load = gvfsmod__module_load,
        .unload = gvfsmod__module_unload,
    };
}

static bool gvfsmod__module_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = GECS_CONTEXT(world, gvfsmod_Service);
    if (type == 0) {
        return false;
    }

    gvfsmod_Service* existing = gecs_context_get(world, type);
    if (existing != NULL) {
        return existing->vfs != NULL;
    }

    gvfsmod_Service service = {.vfs = gvfs_create(alloc), .module_owned = true};
    if (service.vfs == NULL) {
        return false;
    }

    gecs_Component host_type = GECS_CONTEXT(world, gvfsmod_Host);
    if (host_type == 0) {
        goto error;
    }

    const gvfsmod_Host* host = gecs_context_get(world, host_type);
    gvfsmod_Host configured;
    gecs_Component config_type = gecs_context_find(world, "glint_Config");
    const glint_Config* config = config_type != 0 ? gecs_context_get(world, config_type) : NULL;
    if (host == NULL && config != NULL) {
        configured = (gvfsmod_Host) {
            .asset_root = config->asset_root,
            .mounts = config->mounts,
            .mount_count = config->mount_count
        };
        host = &configured;
    }

    if (host != NULL) {
        if (host->mount_count > 0 && host->mounts == NULL) {
            goto error;
        }

        char* working_directory = host->asset_root != NULL ? NULL : gvfs_get_working_directory(alloc);
        if (host->asset_root == NULL && working_directory == NULL) {
            goto error;
        }
        bool mounted =
            gvfs_mount(alloc, service.vfs, "/", host->asset_root != NULL ? host->asset_root : working_directory);
        galloc_free(alloc, working_directory);
        if (!mounted) {
            goto error;
        }

        for (size_t i = 0; i < host->mount_count; i++) {
            const gvfsmod_Mount* mount = &host->mounts[i];
            if (mount->point == NULL || mount->root == NULL
                || !gvfs_mount(alloc, service.vfs, mount->point, mount->root)) {
                goto error;
            }
        }
    }

    if (gecs_context_set(world, type, &service) == NULL) {
        goto error;
    }

    return true;

error:
    gvfs_destroy(alloc, service.vfs);
    return false;
}

static void gvfsmod__module_unload(gecs_World* world, void* user) {
    (void)user;

    galloc_Allocator alloc = glint_allocator(world);
    gecs_Component type = gecs_context_find(world, "gvfsmod_Service");
    gvfsmod_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    if (service != NULL && service->module_owned) {
        gvfs_destroy(alloc, service->vfs);
        gecs_context_remove(world, type);
    }
}

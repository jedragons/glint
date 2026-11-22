#include "./asset.h"

#include <stdint.h>
#include <string.h>

char* gdim2__asset_texture_path(galloc_Allocator alloc, const char* path) {
    size_t length = strlen(path);
    const char* slash = strrchr(path, '/');
    const char* dot = strrchr(path, '.');

    bool has_extension = dot != NULL && (slash == NULL || dot > slash);
    size_t stem = has_extension ? (size_t)(dot - path) : length;
    if (stem > SIZE_MAX - 5) {
        return NULL;
    }

    char* result = galloc_alloc(alloc, stem + 5);
    if (result != NULL) {
        memcpy(result, path, stem);
        memcpy(result + stem, ".png", 5);
    }

    return result;
}

bool gdim2__asset_read(galloc_Allocator alloc, gvfs_Vfs* vfs, const char* path, gdim2_atlas_Atlas* atlas) {
    size_t size = 0;
    char* json = gvfs_read(alloc, vfs, path, &size);
    if (json == NULL) {
        return false;
    }

    bool parsed = gdim2_atlas_parse(alloc, json, size, atlas);
    galloc_free(alloc, json);
    if (!parsed) {
        return false;
    }

    return true;
}

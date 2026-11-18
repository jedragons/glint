#pragma once

#include <glint/modules/assets/cache.h>
#include <glint/modules/render/render.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gtexture_Assets gtexture_Assets;

typedef struct gtexture_Asset {
    grender_Texture texture;
} gtexture_Asset;

#define GTEXTURE_ASSET_TYPE "glint.texture"

/// Register textures with a cache. The cache and VFS must outlive the registration.
gtexture_Assets* gtexture_new(galloc_Allocator alloc, gasset_Cache* cache, gvfs_Vfs* vfs);

/// Unregister texture assets.
///
/// # Errors
///
/// Returns false while cache callbacks or polling are active; retry afterwards.
bool gtexture_destroy(galloc_Allocator alloc, gtexture_Assets* assets);

/// Borrow a cached texture; its address remains stable across reloads.
gtexture_Asset* gtexture_load(galloc_Allocator alloc, gtexture_Assets* assets, const char* path);

bool gtexture_matches(const gtexture_Assets* assets, gasset_Cache* cache, gvfs_Vfs* vfs);

#ifdef __cplusplus
}
#endif

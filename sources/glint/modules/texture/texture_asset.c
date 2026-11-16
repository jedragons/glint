#include <glint/modules/texture/texture_asset.h>

struct gtexture_Assets {
    gasset_Cache* cache;
    gvfs_Vfs* vfs;
};

static bool gtexture__load(galloc_Allocator alloc, void* instance, const char* path, void* user);
static bool gtexture__reload(galloc_Allocator alloc, void* instance, const char* path, void* user);
static void gtexture__unload(galloc_Allocator alloc, void* instance, void* user);

gtexture_Assets* gtexture_new(galloc_Allocator alloc, gasset_Cache* cache, gvfs_Vfs* vfs) {
    if (cache == NULL || vfs == NULL) {
        return NULL;
    }

    gtexture_Assets* assets = galloc_alloc(alloc, sizeof(*assets));
    if (assets == NULL) {
        return NULL;
    }

    *assets = (gtexture_Assets) {.cache = cache, .vfs = vfs};
    if (!gasset_Cache_register(
            alloc,
            cache,
            GTEXTURE_ASSET_TYPE,
            sizeof(gtexture_Asset),
            gtexture__load,
            gtexture__reload,
            gtexture__unload,
            assets
        )) {
        galloc_free(alloc, assets);
        return NULL;
    }

    return assets;
}

bool gtexture_destroy(galloc_Allocator alloc, gtexture_Assets* assets) {
    if (assets == NULL) {
        return true;
    }

    if (!gasset_Cache_unregister(alloc, assets->cache, GTEXTURE_ASSET_TYPE)) {
        return false;
    }

    galloc_free(alloc, assets);
    return true;
}

gtexture_Asset* gtexture_load(galloc_Allocator alloc, gtexture_Assets* assets, const char* path) {
    return assets != NULL ? gasset_Cache_load(alloc, assets->cache, GTEXTURE_ASSET_TYPE, path) : NULL;
}

bool gtexture_matches(const gtexture_Assets* assets, gasset_Cache* cache, gvfs_Vfs* vfs) {
    return assets != NULL && assets->cache == cache && assets->vfs == vfs;
}

static bool gtexture__load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    gtexture_Assets* assets = user;
    gtexture_Asset* asset = instance;
    asset->texture = grender_Texture_load_vfs(alloc, assets->vfs, path);
    if (!grender_Texture_valid(&asset->texture)) {
        return false;
    }

    char* native = gvfs_real_path(alloc, assets->vfs, path);
    bool ok = native == NULL || gasset_Cache_watch(alloc, assets->cache, GTEXTURE_ASSET_TYPE, path, native);
    galloc_free(alloc, native);
    if (!ok) {
        grender_Texture_unload(&asset->texture);
    }

    return ok;
}

static bool gtexture__reload(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    gtexture_Assets* assets = user;
    gtexture_Asset* asset = instance;
    grender_Texture texture = grender_Texture_load_vfs(alloc, assets->vfs, path);
    if (!grender_Texture_valid(&texture)) {
        return false;
    }

    grender_Texture_unload(&asset->texture);
    asset->texture = texture;
    return true;
}

static void gtexture__unload(galloc_Allocator alloc, void* instance, void* user) {
    (void)alloc;
    (void)user;

    gtexture_Asset* asset = instance;
    grender_Texture_unload(&asset->texture);
}

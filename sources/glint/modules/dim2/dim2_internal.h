#pragma once

#include "./atlas.h"
#include "./dim2.h"

#include <glint/modules/texture/texture_asset.h>

typedef struct gdim2_bindings_State gdim2_bindings_State;

typedef struct gdim2__AtlasAsset {
    struct gdim2__AtlasAsset* next;
    char* path;
    bool dirty;
    gdim2_atlas_Atlas atlas;
    gtexture_Asset* texture;
} gdim2__AtlasAsset;

typedef struct gdim2__Sprite {
    gdim2__AtlasAsset* asset;
    const gdim2_atlas_Frame* frame;
} gdim2__Sprite;

typedef struct gdim2__AnimatedSprite {
    gdim2__AtlasAsset* asset;
    gdim2_atlas_AnimationPlayer player;
} gdim2__AnimatedSprite;

struct gdim2_Dim2 {
    gecs_World* world;
    gvfs_Vfs* vfs;
    gecs_Component transform;
    gecs_Component sprite;
    gecs_Component animated;
    gdim2__AtlasAsset* assets;
    gasset_Cache* cache;
    bool owns_cache;
    gtexture_Assets* textures;
    bool owns_textures;
    gdim2_bindings_State* bindings;
};

#ifdef __cplusplus
extern "C" {
#endif

gdim2__AtlasAsset* gdim2__load_asset(galloc_Allocator alloc, gdim2_Dim2* dim2, const char* path);

gdim2_bindings_State* gdim2_bindings__new(galloc_Allocator alloc, gdim2_Dim2* dim2);

void gdim2_bindings__destroy(gdim2_bindings_State* bindings);

#ifdef __cplusplus
}
#endif

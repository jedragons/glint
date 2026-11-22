#include "./asset.h"
#include "./dim2_internal.h"

#include <glint/modules/render/render.h>
#include <glint/modules/texture/module.h>

#include <stdint.h>
#include <string.h>

#define GDIM2_ATLAS_ASSET_TYPE "glint.dim2.atlas"

static char* gdim2__copy_path(galloc_Allocator alloc, const char* path);
static bool gdim2__cache_load(galloc_Allocator alloc, void* instance, const char* path, void* user);
static void gdim2__cache_unload(galloc_Allocator alloc, void* instance, void* user);
static bool gdim2__reload_asset(galloc_Allocator alloc, gdim2_Dim2* dim2, gdim2__AtlasAsset* asset);
static bool gdim2__cache_reload(galloc_Allocator alloc, void* instance, const char* path, void* user);
static void gdim2__draw_frame(
    const gdim2__AtlasAsset* asset,
    const gdim2_atlas_Frame* frame,
    const gdim2_Transform2D* transform
);

gdim2_Dim2* gdim2_new(gecs_World* world, gvfs_Vfs* vfs, gasset_Cache* assets) {
    if (world == NULL || vfs == NULL) {
        return NULL;
    }

    galloc_Allocator alloc = glint_allocator(world);
    gdim2_Dim2* dim2 = galloc_calloc(alloc, 1, sizeof(*dim2));
    if (dim2 == NULL) {
        return NULL;
    }

    dim2->bindings = gdim2_bindings__new(alloc, dim2);
    if (dim2->bindings == NULL) {
        galloc_free(alloc, dim2);
        return NULL;
    }

    dim2->world = world;
    dim2->vfs = vfs;
    dim2->owns_cache = assets == NULL;
    dim2->cache = assets != NULL ? assets : gasset_Cache_create(alloc);
    gecs_Component texture_type = GECS_CONTEXT(world, gtexture_Service);
    gtexture_Service* texture_service = texture_type != 0 ? gecs_context_get(world, texture_type) : NULL;
    if (texture_service != NULL) {
        if (gtexture_matches(texture_service->assets, dim2->cache, vfs)) {
            dim2->textures = texture_service->assets;
        }
    } else if (dim2->cache != NULL) {
        dim2->textures = gtexture_new(alloc, dim2->cache, vfs);
        dim2->owns_textures = dim2->textures != NULL;
    }

    dim2->transform = GECS_COMPONENT(world, gdim2_Transform2D);
    dim2->sprite = GECS_COMPONENT(world, gdim2__Sprite);
    dim2->animated = GECS_COMPONENT(world, gdim2__AnimatedSprite);
    bool components_registered = dim2->transform != 0 && dim2->sprite != 0 && dim2->animated != 0;
    bool resources_ready = dim2->cache != NULL && dim2->textures != NULL;
    bool atlas_registered =
        components_registered && resources_ready
        && gasset_Cache_register(
            alloc,
            dim2->cache,
            GDIM2_ATLAS_ASSET_TYPE,
            sizeof(gdim2__AtlasAsset),
            gdim2__cache_load,
            gdim2__cache_reload,
            gdim2__cache_unload,
            dim2
        );

    if (!atlas_registered) {
        if (dim2->owns_textures) {
            gtexture_destroy(alloc, dim2->textures);
        }

        if (dim2->owns_cache) {
            gasset_Cache_destroy(alloc, dim2->cache);
        }

        gdim2_bindings__destroy(dim2->bindings);
        galloc_free(alloc, dim2);
        return NULL;
    }

    return dim2;
}

bool gdim2_destroy(galloc_Allocator alloc, gdim2_Dim2* dim2) {
    if (dim2 == NULL) {
        return true;
    }

    // The cache cannot unregister while a callback or poll is active. Keep
    // the instance intact so its owner can retry after that operation.
    if (!gasset_Cache_unregister(alloc, dim2->cache, GDIM2_ATLAS_ASSET_TYPE)) {
        return false;
    }

    gdim2_bindings__destroy(dim2->bindings);
    while (gecs_Component_count(dim2->world, dim2->sprite) > 0) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->sprite, 0);
        gecs_Component_remove(e, dim2->sprite);
    }

    while (gecs_Component_count(dim2->world, dim2->animated) > 0) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->animated, 0);
        gecs_Component_remove(e, dim2->animated);
    }

    if (dim2->owns_textures) {
        gtexture_destroy(alloc, dim2->textures);
    }

    if (dim2->owns_cache) {
        gasset_Cache_destroy(alloc, dim2->cache);
    }

    galloc_free(alloc, dim2);
    return true;
}

bool gdim2_busy(const gdim2_Dim2* dim2) {
    return dim2 != NULL && gasset_Cache_busy(dim2->cache);
}

void gdim2_update(galloc_Allocator alloc, gdim2_Dim2* dim2, float delta) {
    if (dim2 == NULL) {
        return;
    }

    if (dim2->owns_cache) {
        gasset_Cache_poll(alloc, dim2->cache);
    }

    for (gdim2__AtlasAsset* asset = dim2->assets; asset != NULL; asset = asset->next) {
        if (asset->dirty) {
            gasset_Cache_reload(alloc, dim2->cache, GDIM2_ATLAS_ASSET_TYPE, asset->path);
        }
    }

    size_t count = gecs_Component_count(dim2->world, dim2->animated);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite* animated = gecs_Component_get(entity, dim2->animated);
        gdim2_atlas_AnimationPlayer_update(&animated->player, delta);
    }
}

void gdim2_render(gdim2_Dim2* dim2) {
    if (dim2 == NULL) {
        return;
    }

    size_t count = gecs_Component_count(dim2->world, dim2->sprite);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(dim2->world, dim2->sprite, i);
        gdim2__Sprite* sprite = gecs_Component_get(entity, dim2->sprite);
        gdim2_Transform2D* transform = gecs_Component_get(entity, dim2->transform);
        gdim2__draw_frame(sprite->asset, sprite->frame, transform);
    }

    count = gecs_Component_count(dim2->world, dim2->animated);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite* animated = gecs_Component_get(entity, dim2->animated);
        gdim2_Transform2D* transform = gecs_Component_get(entity, dim2->transform);
        gdim2__draw_frame(animated->asset, gdim2_atlas_AnimationPlayer_frame(&animated->player), transform);
    }
}

gdim2__AtlasAsset* gdim2__load_asset(galloc_Allocator alloc, gdim2_Dim2* dim2, const char* path) {
    return gasset_Cache_load(alloc, dim2->cache, GDIM2_ATLAS_ASSET_TYPE, path);
}

static char* gdim2__copy_path(galloc_Allocator alloc, const char* path) {
    size_t length = strlen(path);
    if (length == SIZE_MAX) {
        return NULL;
    }

    char* copy = galloc_alloc(alloc, length + 1);
    if (copy != NULL) {
        memcpy(copy, path, length + 1);
    }

    return copy;
}

static bool gdim2__cache_load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    gdim2_Dim2* dim2 = user;
    gdim2__AtlasAsset* asset = instance;
    asset->path = gdim2__copy_path(alloc, path);
    char* image_path = gdim2__asset_texture_path(alloc, path);
    char* json_real = gvfs_real_path(alloc, dim2->vfs, path);
    bool paths_ready = asset->path != NULL && image_path != NULL;
    bool atlas_loaded = paths_ready && gdim2__asset_read(alloc, dim2->vfs, path, &asset->atlas);
    if (atlas_loaded) {
        asset->texture = gtexture_load(alloc, dim2->textures, image_path);
    }

    bool texture_loaded = atlas_loaded && asset->texture != NULL;
    bool ok = texture_loaded
        && (json_real == NULL || gasset_Cache_watch(alloc, dim2->cache, GDIM2_ATLAS_ASSET_TYPE, path, json_real));

    galloc_free(alloc, image_path);
    galloc_free(alloc, json_real);

    if (!ok) {
        gdim2_atlas_destroy(alloc, &asset->atlas);
        galloc_free(alloc, asset->path);
        return false;
    }

    asset->next = dim2->assets;
    dim2->assets = asset;
    return true;
}

static void gdim2__cache_unload(galloc_Allocator alloc, void* instance, void* user) {
    gdim2_Dim2* dim2 = user;
    gdim2__AtlasAsset* asset = instance;
    gdim2__AtlasAsset** link = &dim2->assets;

    while (*link != NULL && *link != asset) {
        link = &(*link)->next;
    }

    if (*link != NULL) {
        *link = asset->next;
    }

    gdim2_atlas_destroy(alloc, &asset->atlas);
    galloc_free(alloc, asset->path);
}

static bool gdim2__reload_asset(galloc_Allocator alloc, gdim2_Dim2* dim2, gdim2__AtlasAsset* asset) {
    gdim2_atlas_Atlas atlas = {0};
    if (!gdim2__asset_read(alloc, dim2->vfs, asset->path, &atlas)) {
        return false;
    }

    size_t sprites = gecs_Component_count(dim2->world, dim2->sprite);
    size_t animations = gecs_Component_count(dim2->world, dim2->animated);

    // Keep the old asset intact when a partially-written file removes a live
    // name; a later edit can make the reload succeed.
    for (size_t i = 0; i < sprites; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->sprite, i);
        gdim2__Sprite* sprite = gecs_Component_get(e, dim2->sprite);
        if (sprite->asset == asset && gdim2_atlas_frame(&atlas, sprite->frame->name) == NULL) {
            goto error;
        }
    }

    for (size_t i = 0; i < animations; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite* sprite = gecs_Component_get(e, dim2->animated);
        if (sprite->asset == asset && gdim2_atlas_animation(&atlas, sprite->player.animation->name) == NULL) {
            goto error;
        }
    }

    for (size_t i = 0; i < sprites; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->sprite, i);
        gdim2__Sprite* sprite = gecs_Component_get(e, dim2->sprite);
        if (sprite->asset == asset) {
            sprite->frame = gdim2_atlas_frame(&atlas, sprite->frame->name);
        }
    }

    for (size_t i = 0; i < animations; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite* sprite = gecs_Component_get(e, dim2->animated);
        if (sprite->asset == asset) {
            sprite->player.animation = gdim2_atlas_animation(&atlas, sprite->player.animation->name);
            if (sprite->player.current_frame >= sprite->player.animation->count) {
                sprite->player.current_frame = 0;
                sprite->player.time = 0;
            }
        }
    }

    gdim2_atlas_destroy(alloc, &asset->atlas);
    asset->atlas = atlas;
    return true;

error:
    gdim2_atlas_destroy(alloc, &atlas);
    return false;
}

static bool gdim2__cache_reload(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    (void)path;
    gdim2__AtlasAsset* asset = instance;
    bool ok = gdim2__reload_asset(alloc, user, asset);
    asset->dirty = !ok;
    return ok;
}

static void gdim2__draw_frame(
    const gdim2__AtlasAsset* asset,
    const gdim2_atlas_Frame* frame,
    const gdim2_Transform2D* transform
) {
    if (frame == NULL || transform == NULL) {
        return;
    }

    grender_draw_texture_params(
        asset->texture->texture,
        (grender_TextureDrawParams) {
            .source =
                {
                    .x = frame->x,
                    .y = frame->y,
                    .w = frame->width,
                    .h = frame->height,
                },
            .dest =
                {
                    .x = transform->position.x,
                    .y = transform->position.y,
                    .w = frame->width * transform->scale.x,
                    .h = frame->height * transform->scale.y,
                },
            .origin =
                {
                    .x = transform->origin.x,
                    .y = transform->origin.y,
                },
            .rotation = transform->rotation,
            .tint = {255, 255, 255, 255},
        }
    );
}

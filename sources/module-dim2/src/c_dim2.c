#include <glint/dim2/dim2.h>
#include <glint/dim2/atlas.h>
#include <glint/assets/file_watcher.h>
#include <glint/script_entity.h>
#include <glint/types_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <raylib.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct gdim2__Dim2Bindings gdim2__Dim2Bindings;

typedef struct gdim2__AtlasAsset {
    struct gdim2__AtlasAsset *next;
    char *path;
    char *json_real_path;
    char *image_real_path;
    bool dirty;
    gatlas_Atlas atlas;
    Texture2D texture;
} gdim2__AtlasAsset;

typedef struct gdim2__Sprite {
    gdim2__AtlasAsset *asset;
    const gatlas_Frame *frame;
} gdim2__Sprite;

typedef struct gdim2__AnimatedSprite {
    gdim2__AtlasAsset *asset;
    gatlas_AnimationPlayer player;
} gdim2__AnimatedSprite;

struct gdim2_Dim2 {
    gecs_World *world;
    gvfs_Vfs *vfs;
    gecs_Component transform;
    gecs_Component sprite;
    gecs_Component animated;
    gdim2__AtlasAsset *assets;
    gwatch_Watcher *watcher;
    gdim2__Dim2Bindings *gdim2__dim2_bindings;
};

struct gdim2__Dim2Bindings {
    galloc_Allocator alloc;
    gdim2_Dim2 *dim2;
    size_t references;
};

#define GDIM2_DIM2_BINDINGS_META "glint.Dim2Bindings"
#define GDIM2_TRANSFORM_META "glint.Transform2D"
#define GDIM2_TRANSFORM_VEC_META "glint.Transform2D.Vector"
#define GDIM2_ATLAS_META "glint.Atlas"
#define GDIM2_SPRITE_VALUE_META "glint.SpriteValue"
#define GDIM2_ANIMATED_VALUE_META "glint.AnimatedSpriteValue"

typedef struct gdim2__TransformRef {
    gdim2__Dim2Bindings *gdim2__dim2_bindings;
    /* Standalone Transform2D.new() values point into storage bytes stored
     * directly after this struct; entity references leave it NULL. */
    gdim2_Transform2D *storage;
    gecs_Entity entity;
    int vector_field;
} gdim2__TransformRef;

typedef struct gdim2__AtlasValue {
    gdim2__AtlasAsset *asset;
} gdim2__AtlasValue;

/* Standalone sprite values remember the frame name and re-resolve it on use,
 * so they survive atlas reloads that preserve the name. */
typedef struct gdim2__SpriteValue {
    gdim2__AtlasAsset *asset;
    const char *name; /* Points into bytes stored after this struct. */
} gdim2__SpriteValue;

typedef struct gdim2__AnimatedSpriteValue {
    gdim2__AtlasAsset *asset;
    const char *name; /* Points into bytes stored after this struct. */
    size_t current_frame;
    float time;
} gdim2__AnimatedSpriteValue;

static void gdim2__dim2_release_bindings(gdim2__Dim2Bindings *gdim2__dim2_bindings) {
    if (--gdim2__dim2_bindings->references == 0) {
        galloc_free(gdim2__dim2_bindings->alloc, gdim2__dim2_bindings);
    }
}

static int gdim2__dim2_bindings_gc(lua_State *lua) {
    gdim2__Dim2Bindings **handle = luaL_checkudata(lua, 1, GDIM2_DIM2_BINDINGS_META);
    if (*handle) {
        gdim2__dim2_release_bindings(*handle);
        *handle = NULL;
    }
    return 0;
}

static char *gdim2__dim2_copy_path(galloc_Allocator alloc, const char *path) {
    size_t length = strlen(path);
    if (length == SIZE_MAX) {
        return NULL;
    }
    char *copy = galloc_alloc(alloc, length + 1);
    if (copy) {
        memcpy(copy, path, length + 1);
    }
    return copy;
}

static char *gdim2__dim2_texture_path(galloc_Allocator alloc, const char *path) {
    size_t length = strlen(path);
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    size_t stem = dot && (!slash || dot > slash) ? (size_t)(dot - path) : length;
    if (stem > SIZE_MAX - 5) {
        return NULL;
    }
    char *result = galloc_alloc(alloc, stem + 5);
    if (result) {
        memcpy(result, path, stem);
        memcpy(result + stem, ".png", 5);
    }
    return result;
}

static bool gdim2__dim2_read_asset(galloc_Allocator alloc, gdim2_Dim2 *dim2, const char *path, gatlas_Atlas *atlas, Texture2D *texture) {
    size_t size = 0;
    char *json = gvfs_read(alloc, dim2->vfs, path, &size);
    if (!json) {
        return false;
    }
    bool parsed = gatlas_parse(alloc, json, size, atlas);
    galloc_free(alloc, json);
    if (!parsed) {
        return false;
    }
    char *image_path = gdim2__dim2_texture_path(alloc, path);
    unsigned char *bytes = image_path ? gvfs_read(alloc, dim2->vfs, image_path, &size) : NULL;
    galloc_free(alloc, image_path);
    if (bytes && size <= INT_MAX) {
        Image image = LoadImageFromMemory(".png", bytes, (int)size);
        if (IsImageValid(image)) {
            *texture = LoadTextureFromImage(image);
            UnloadImage(image);
        }
    }
    galloc_free(alloc, bytes);
    if (!IsTextureValid(*texture)) {
        gatlas_destroy(alloc, atlas);
        return false;
    }
    return true;
}

static gdim2__AtlasAsset *gdim2__dim2_load_asset(galloc_Allocator alloc, gdim2_Dim2 *dim2, const char *path) {
    for (gdim2__AtlasAsset *asset = dim2->assets; asset; asset = asset->next) {
        if (strcmp(asset->path, path) == 0) {
            return asset;
        }
    }
    gdim2__AtlasAsset *asset = galloc_calloc(alloc, 1, sizeof(*asset));
    if (!asset) {
        return NULL;
    }
    asset->path = gdim2__dim2_copy_path(alloc, path);
    char *image_path = gdim2__dim2_texture_path(alloc, path);
    asset->json_real_path = gvfs_real_path(alloc, dim2->vfs, path);
    asset->image_real_path = image_path ? gvfs_real_path(alloc, dim2->vfs, image_path) : NULL;
    galloc_free(alloc, image_path);
    if (!asset->path || !asset->json_real_path || !asset->image_real_path
        || !gdim2__dim2_read_asset(alloc, dim2, path, &asset->atlas, &asset->texture)
        || !gwatch_add(alloc, dim2->watcher, asset->json_real_path)
        || !gwatch_add(alloc, dim2->watcher, asset->image_real_path)) {
        if (IsTextureValid(asset->texture)) {
            UnloadTexture(asset->texture);
        }
        gatlas_destroy(alloc, &asset->atlas);
        galloc_free(alloc, asset->json_real_path);
        galloc_free(alloc, asset->image_real_path);
        galloc_free(alloc, asset->path);
        galloc_free(alloc, asset);
        return NULL;
    }
    asset->next = dim2->assets;
    dim2->assets = asset;
    return asset;
}

static bool gdim2__dim2_reload_asset(galloc_Allocator alloc, gdim2_Dim2 *dim2, gdim2__AtlasAsset *asset) {
    gatlas_Atlas atlas = {0};
    Texture2D texture = {0};
    if (!gdim2__dim2_read_asset(alloc, dim2, asset->path, &atlas, &texture)) {
        return false;
    }
    size_t sprites = gecs_Component_count(dim2->world, dim2->sprite);
    size_t animations = gecs_Component_count(dim2->world, dim2->animated);
    /* Keep the old asset intact when a partially-written file removes a live
     * name; a later edit can make the reload succeed. */
    for (size_t i = 0; i < sprites; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->sprite, i);
        gdim2__Sprite *sprite = gecs_Component_get(dim2->world, e, dim2->sprite);
        if (sprite->asset == asset && !gatlas_frame(&atlas, sprite->frame->name)) {
            goto fail;
        }
    }
    for (size_t i = 0; i < animations; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite *sprite = gecs_Component_get(dim2->world, e, dim2->animated);
        if (sprite->asset == asset && !gatlas_animation(&atlas, sprite->player.animation->name)) {
            goto fail;
        }
    }
    for (size_t i = 0; i < sprites; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->sprite, i);
        gdim2__Sprite *sprite = gecs_Component_get(dim2->world, e, dim2->sprite);
        if (sprite->asset == asset) {
            sprite->frame = gatlas_frame(&atlas, sprite->frame->name);
        }
    }
    for (size_t i = 0; i < animations; i++) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite *sprite = gecs_Component_get(dim2->world, e, dim2->animated);
        if (sprite->asset == asset) {
            sprite->player.animation = gatlas_animation(&atlas, sprite->player.animation->name);
            if (sprite->player.current_frame >= sprite->player.animation->count) {
                sprite->player.current_frame = 0;
                sprite->player.time = 0;
            }
        }
    }
    UnloadTexture(asset->texture);
    gatlas_destroy(alloc, &asset->atlas);
    asset->atlas = atlas;
    asset->texture = texture;
    return true;
fail:
    UnloadTexture(texture);
    gatlas_destroy(alloc, &atlas);
    return false;
}

static void gdim2__dim2_asset_changed(const char *path, void *user) {
    gdim2_Dim2 *dim2 = user;
    for (gdim2__AtlasAsset *asset = dim2->assets; asset; asset = asset->next) {
        if (strcmp(asset->json_real_path, path) == 0 || strcmp(asset->image_real_path, path) == 0) {
            asset->dirty = true;
        }
    }
}

gdim2_Dim2 *gdim2_new(galloc_Allocator alloc, gecs_World *world, gvfs_Vfs *vfs) {
    if (!world || !vfs) {
        return NULL;
    }
    gdim2_Dim2 *dim2 = galloc_calloc(alloc, 1, sizeof(*dim2));
    if (!dim2) {
        return NULL;
    }
    dim2->gdim2__dim2_bindings = galloc_calloc(alloc, 1, sizeof(*dim2->gdim2__dim2_bindings));
    if (!dim2->gdim2__dim2_bindings) {
        galloc_free(alloc, dim2);
        return NULL;
    }
    *dim2->gdim2__dim2_bindings = (gdim2__Dim2Bindings) {.alloc = alloc, .dim2 = dim2, .references = 1};
    dim2->world = world;
    dim2->vfs = vfs;
    dim2->watcher = gwatch_create(alloc);
    dim2->transform = GECS_COMPONENT(alloc, world, gdim2_Transform2D);
    dim2->sprite = GECS_COMPONENT(alloc, world, gdim2__Sprite);
    dim2->animated = GECS_COMPONENT(alloc, world, gdim2__AnimatedSprite);
    if (!dim2->transform || !dim2->sprite || !dim2->animated || !dim2->watcher) {
        gwatch_destroy(alloc, dim2->watcher);
        gdim2__dim2_release_bindings(dim2->gdim2__dim2_bindings);
        galloc_free(alloc, dim2);
        return NULL;
    }
    return dim2;
}

void gdim2_destroy(galloc_Allocator alloc, gdim2_Dim2 *dim2) {
    if (!dim2) {
        return;
    }
    dim2->gdim2__dim2_bindings->dim2 = NULL;
    while (gecs_Component_count(dim2->world, dim2->sprite)) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->sprite, 0);
        gecs_Component_remove(dim2->world, e, dim2->sprite);
    }
    while (gecs_Component_count(dim2->world, dim2->animated)) {
        gecs_Entity e = gecs_Component_entity(dim2->world, dim2->animated, 0);
        gecs_Component_remove(dim2->world, e, dim2->animated);
    }
    gdim2__AtlasAsset *asset = dim2->assets;
    while (asset) {
        gdim2__AtlasAsset *next = asset->next;
        UnloadTexture(asset->texture);
        gatlas_destroy(alloc, &asset->atlas);
        galloc_free(alloc, asset->path);
        galloc_free(alloc, asset->json_real_path);
        galloc_free(alloc, asset->image_real_path);
        galloc_free(alloc, asset);
        asset = next;
    }
    gwatch_destroy(alloc, dim2->watcher);
    gdim2__dim2_release_bindings(dim2->gdim2__dim2_bindings);
    galloc_free(alloc, dim2);
}

void gdim2_update(galloc_Allocator alloc, gdim2_Dim2 *dim2, float delta) {
    if (!dim2) {
        return;
    }
    gwatch_poll(dim2->watcher, gdim2__dim2_asset_changed, dim2);
    for (gdim2__AtlasAsset *asset = dim2->assets; asset; asset = asset->next) {
        if (asset->dirty && gdim2__dim2_reload_asset(alloc, dim2, asset)) {
            asset->dirty = false;
        }
    }
    size_t count = gecs_Component_count(dim2->world, dim2->animated);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite *animated = gecs_Component_get(dim2->world, entity, dim2->animated);
        gatlas_AnimationPlayer_update(&animated->player, delta);
    }
}

static void gdim2__dim2_draw_frame(const gdim2__AtlasAsset *asset, const gatlas_Frame *frame, const gdim2_Transform2D *transform) {
    if (!frame || !transform) {
        return;
    }
    Rectangle source = {frame->x, frame->y, frame->width, frame->height};
    Rectangle dest = {
        transform->position.x,
        transform->position.y,
        frame->width * transform->scale.x,
        frame->height * transform->scale.y
    };
    Vector2 origin = {transform->origin.x, transform->origin.y};
    DrawTexturePro(asset->texture, source, dest, origin, transform->rotation, WHITE);
}

void gdim2_render(gdim2_Dim2 *dim2) {
    if (!dim2) {
        return;
    }
    size_t count = gecs_Component_count(dim2->world, dim2->sprite);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(dim2->world, dim2->sprite, i);
        gdim2__Sprite *sprite = gecs_Component_get(dim2->world, entity, dim2->sprite);
        gdim2_Transform2D *transform = gecs_Component_get(dim2->world, entity, dim2->transform);
        gdim2__dim2_draw_frame(sprite->asset, sprite->frame, transform);
    }
    count = gecs_Component_count(dim2->world, dim2->animated);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(dim2->world, dim2->animated, i);
        gdim2__AnimatedSprite *animated = gecs_Component_get(dim2->world, entity, dim2->animated);
        gdim2_Transform2D *transform = gecs_Component_get(dim2->world, entity, dim2->transform);
        gdim2__dim2_draw_frame(animated->asset, gatlas_AnimationPlayer_frame(&animated->player), transform);
    }
}

static gdim2__Dim2Bindings *gdim2__dim2_bindings(lua_State *lua) {
    gdim2__Dim2Bindings **handle = lua_touserdata(lua, lua_upvalueindex(1));
    if (!handle || !*handle || !(*handle)->dim2) {
        luaL_error(lua, "2D module is no longer available");
    }
    return *handle;
}

static gecs_Entity gdim2__dim2_check_entity(lua_State *lua, gdim2__Dim2Bindings *ctx, int index) {
    gecs_World *world = NULL;
    gecs_Entity entity = 0;
    gscript_check_entity(lua, index, &world, &entity);
    if (world != ctx->dim2->world || !gecs_Entity_alive(world, entity)) {
        luaL_error(lua, "Entity is invalid or belongs to a different world");
    }
    return entity;
}

static gdim2_Transform2D *gdim2__dim2_get_transform(lua_State *lua, gdim2__Dim2Bindings *ctx, gecs_Entity entity) {
    if (!gecs_Entity_alive(ctx->dim2->world, entity)) {
        luaL_error(lua, "Entity is no longer valid");
    }
    gdim2_Transform2D *transform = gecs_Component_get(ctx->dim2->world, entity, ctx->dim2->transform);
    if (!transform) {
        luaL_error(lua, "Transform2D component no longer exists");
    }
    return transform;
}

static gvec2_Vec2 *gdim2__dim2_transform_vector(gdim2_Transform2D *transform, int field) {
    if (field == 0) {
        return &transform->position;
    }
    if (field == 1) {
        return &transform->scale;
    }
    return &transform->origin;
}

static gdim2__Dim2Bindings *gdim2__dim2_ref_bindings(lua_State *lua, int index);

/* Standalone values resolve to their own storage; entity references resolve
 * through the ECS and error when the component is gone. */
static gdim2_Transform2D *gdim2__dim2_ref_transform(lua_State *lua, gdim2__Dim2Bindings *ctx, gdim2__TransformRef *ref) {
    if (ref->storage) {
        return ref->storage;
    }
    return gdim2__dim2_get_transform(lua, ctx, ref->entity);
}

static gvec2_Vec2 gdim2__dim2_check_vec2(lua_State *lua, gdim2__Dim2Bindings *ctx, int index) {
    gdim2__TransformRef *ref = luaL_testudata(lua, index, GDIM2_TRANSFORM_VEC_META);
    if (ref) {
        if (gdim2__dim2_ref_bindings(lua, index)->dim2 != ctx->dim2) {
            luaL_error(lua, "Vec2 belongs to a different 2D module");
        }
        return *gdim2__dim2_transform_vector(gdim2__dim2_ref_transform(lua, ctx, ref), ref->vector_field);
    }
    luaL_checktype(lua, index, LUA_TTABLE);
    lua_getfield(lua, index, "x");
    float x = (float)luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "y");
    float y = (float)luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    return (gvec2_Vec2) {x, y};
}

static void gdim2__dim2_push_ref(
    lua_State *lua, gdim2__Dim2Bindings *gdim2__dim2_bindings, gecs_Entity entity, gdim2_Transform2D *storage, int field,
    const char *metatable, int binding_index
) {
    gdim2__TransformRef *ref = lua_newuserdatauv(lua, sizeof(*ref), 1);
    *ref = (gdim2__TransformRef) {
        .gdim2__dim2_bindings = gdim2__dim2_bindings,
        .storage = storage,
        .entity = entity,
        .vector_field = field
    };
    luaL_setmetatable(lua, metatable);
    /* The parent object in binding_index keeps this reference alive; for
     * entity references it is the bindings handle, for values the owner. */
    lua_pushvalue(lua, binding_index);
    lua_setiuservalue(lua, -2, 1);
}

/* Reference uservalues either hold the bindings handle or a parent transform
 * reference, which itself keeps the bindings alive. */
static gdim2__Dim2Bindings *gdim2__dim2_ref_bindings(lua_State *lua, int index) {
    lua_getiuservalue(lua, index, 1);
    gdim2__Dim2Bindings *result = NULL;
    gdim2__Dim2Bindings **handle = luaL_testudata(lua, -1, GDIM2_DIM2_BINDINGS_META);
    if (handle) {
        result = *handle;
    } else {
        gdim2__TransformRef *ref = luaL_testudata(lua, -1, GDIM2_TRANSFORM_META);
        result = ref ? ref->gdim2__dim2_bindings : NULL;
    }
    lua_pop(lua, 1);
    if (!result || !result->dim2) {
        luaL_error(lua, "2D module is no longer available");
    }
    return result;
}

static int gdim2__dim2_vector_index(lua_State *lua) {
    gdim2__TransformRef *ref = luaL_checkudata(lua, 1, GDIM2_TRANSFORM_VEC_META);
    gdim2__Dim2Bindings *ctx = gdim2__dim2_ref_bindings(lua, 1);
    const char *key = luaL_checkstring(lua, 2);
    gvec2_Vec2 value = *gdim2__dim2_transform_vector(gdim2__dim2_ref_transform(lua, ctx, ref), ref->vector_field);
    if (strcmp(key, "x") == 0) {
        lua_pushnumber(lua, value.x);
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(lua, value.y);
    } else if (strcmp(key, "scale") == 0) {
        luaL_getmetatable(lua, GDIM2_TRANSFORM_VEC_META);
        lua_getfield(lua, -1, "__vec2_scale");
        lua_remove(lua, -2);
    } else {
        lua_pushnil(lua);
    }
    return 1;
}

static int gdim2__dim2_vector_newindex(lua_State *lua) {
    gdim2__TransformRef *ref = luaL_checkudata(lua, 1, GDIM2_TRANSFORM_VEC_META);
    gdim2__Dim2Bindings *ctx = gdim2__dim2_ref_bindings(lua, 1);
    const char *key = luaL_checkstring(lua, 2);
    float value = (float)luaL_checknumber(lua, 3);
    gvec2_Vec2 *vector = gdim2__dim2_transform_vector(gdim2__dim2_ref_transform(lua, ctx, ref), ref->vector_field);
    if (strcmp(key, "x") == 0) {
        vector->x = value;
    } else if (strcmp(key, "y") == 0) {
        vector->y = value;
    } else {
        return luaL_error(lua, "unknown Vec2 field '%s'", key);
    }
    return 0;
}

static int gdim2__dim2_transform_index(lua_State *lua) {
    gdim2__TransformRef *ref = luaL_checkudata(lua, 1, GDIM2_TRANSFORM_META);
    gdim2__Dim2Bindings *ctx = gdim2__dim2_ref_bindings(lua, 1);
    const char *key = luaL_checkstring(lua, 2);
    gdim2_Transform2D *transform = gdim2__dim2_ref_transform(lua, ctx, ref);
    if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(lua, transform->rotation);
    } else if (strcmp(key, "position") == 0 || strcmp(key, "scale") == 0 || strcmp(key, "origin") == 0) {
        int field = strcmp(key, "position") == 0 ? 0 : strcmp(key, "scale") == 0 ? 1 : 2;
        gdim2__dim2_push_ref(lua, ctx, ref->entity, ref->storage, field, GDIM2_TRANSFORM_VEC_META, 1);
    } else {
        lua_pushnil(lua);
    }
    return 1;
}

static int gdim2__dim2_transform_newindex(lua_State *lua) {
    gdim2__TransformRef *ref = luaL_checkudata(lua, 1, GDIM2_TRANSFORM_META);
    gdim2__Dim2Bindings *ctx = gdim2__dim2_ref_bindings(lua, 1);
    const char *key = luaL_checkstring(lua, 2);
    if (strcmp(key, "rotation") == 0) {
        float value = (float)luaL_checknumber(lua, 3);
        gdim2__dim2_ref_transform(lua, ctx, ref)->rotation = value;
    } else if (strcmp(key, "position") == 0 || strcmp(key, "scale") == 0 || strcmp(key, "origin") == 0) {
        gvec2_Vec2 value = gdim2__dim2_check_vec2(lua, ctx, 3);
        int field = strcmp(key, "position") == 0 ? 0 : strcmp(key, "scale") == 0 ? 1 : 2;
        *gdim2__dim2_transform_vector(gdim2__dim2_ref_transform(lua, ctx, ref), field) = value;
    } else {
        return luaL_error(lua, "unknown Transform2D field '%s'", key);
    }
    return 0;
}

/* Creates a standalone value; optional arguments mirror the old sol2
 * constructors: no arguments, or position, scale, origin, rotation. */
static int gdim2__dim2_transform_new(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    int top = lua_gettop(lua);
    if (top && top != 4) {
        return luaL_error(lua, "Transform2D.new expects no arguments or position, scale, origin, rotation");
    }
    gdim2__TransformRef *ref = lua_newuserdatauv(lua, sizeof(*ref) + sizeof(gdim2_Transform2D), 1);
    gdim2_Transform2D *storage = (gdim2_Transform2D *)(ref + 1);
    *storage = (gdim2_Transform2D) {.scale = {1, 1}};
    *ref = (gdim2__TransformRef) {.gdim2__dim2_bindings = ctx, .storage = storage};
    luaL_setmetatable(lua, GDIM2_TRANSFORM_META);
    if (top) {
        storage->position = gdim2__dim2_check_vec2(lua, ctx, 1);
        storage->scale = gdim2__dim2_check_vec2(lua, ctx, 2);
        storage->origin = gdim2__dim2_check_vec2(lua, ctx, 3);
        storage->rotation = (float)luaL_checknumber(lua, 4);
    }
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int gdim2__dim2_transform_set(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    gdim2_Transform2D transform = {.scale = {1, 1}};
    gdim2__TransformRef *ref = luaL_testudata(lua, 2, GDIM2_TRANSFORM_META);
    if (ref) {
        if (gdim2__dim2_ref_bindings(lua, 2)->dim2 != ctx->dim2) {
            return luaL_error(lua, "Transform2D belongs to a different 2D module");
        }
        transform = *gdim2__dim2_ref_transform(lua, ctx, ref);
    } else if (!lua_isnoneornil(lua, 2)) {
        transform.position = gdim2__dim2_check_vec2(lua, ctx, 2);
        transform.scale = gdim2__dim2_check_vec2(lua, ctx, 3);
        transform.origin = gdim2__dim2_check_vec2(lua, ctx, 4);
        transform.rotation = (float)luaL_checknumber(lua, 5);
    }
    if (!gecs_Component_set(ctx->alloc, ctx->dim2->world, entity, ctx->dim2->transform, &transform)) {
        return luaL_error(lua, "could not allocate Transform2D component");
    }
    gdim2__dim2_push_ref(lua, ctx, entity, NULL, 0, GDIM2_TRANSFORM_META, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_transform_get(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    (void)gdim2__dim2_get_transform(lua, ctx, entity);
    gdim2__dim2_push_ref(lua, ctx, entity, NULL, 0, GDIM2_TRANSFORM_META, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_component_has(lua_State *lua, gecs_Component component) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    lua_pushboolean(lua, gecs_Component_get(ctx->dim2->world, entity, component) != NULL);
    return 1;
}

static int gdim2__dim2_component_remove(lua_State *lua, gecs_Component component) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    gecs_Component_remove(ctx->dim2->world, entity, component);
    return 0;
}

static int gdim2__dim2_transform_has(lua_State *lua) {
    return gdim2__dim2_component_has(lua, gdim2__dim2_bindings(lua)->dim2->transform);
}

static int gdim2__dim2_transform_remove(lua_State *lua) {
    return gdim2__dim2_component_remove(lua, gdim2__dim2_bindings(lua)->dim2->transform);
}

static int gdim2__dim2_sprite_has(lua_State *lua) {
    return gdim2__dim2_component_has(lua, gdim2__dim2_bindings(lua)->dim2->sprite);
}

static int gdim2__dim2_sprite_remove(lua_State *lua) {
    return gdim2__dim2_component_remove(lua, gdim2__dim2_bindings(lua)->dim2->sprite);
}

static int gdim2__dim2_animated_has(lua_State *lua) {
    return gdim2__dim2_component_has(lua, gdim2__dim2_bindings(lua)->dim2->animated);
}

static int gdim2__dim2_animated_remove(lua_State *lua) {
    return gdim2__dim2_component_remove(lua, gdim2__dim2_bindings(lua)->dim2->animated);
}

/* Standalone values must not outlive the atlas they came from; every use
 * re-validates the asset against the module's live list. */
static void gdim2__dim2_check_asset(lua_State *lua, gdim2__Dim2Bindings *ctx, gdim2__AtlasAsset *asset) {
    for (gdim2__AtlasAsset *current = ctx->dim2->assets; current; current = current->next) {
        if (current == asset) {
            return;
        }
    }
    luaL_error(lua, "Atlas is no longer available");
}

static void gdim2__dim2_push_sprite_value(lua_State *lua, gdim2__AtlasAsset *asset, const char *name, int binding_index) {
    size_t length = strlen(name);
    gdim2__SpriteValue *value = lua_newuserdatauv(lua, sizeof(*value) + length + 1, 1);
    char *storage = (char *)(value + 1);
    memcpy(storage, name, length + 1);
    *value = (gdim2__SpriteValue) {.asset = asset, .name = storage};
    luaL_setmetatable(lua, GDIM2_SPRITE_VALUE_META);
    lua_pushvalue(lua, binding_index);
    lua_setiuservalue(lua, -2, 1);
}

static void gdim2__dim2_push_animated_value(lua_State *lua, gdim2__AtlasAsset *asset, const gatlas_AnimationPlayer *player, int binding_index) {
    size_t length = strlen(player->animation->name);
    gdim2__AnimatedSpriteValue *value = lua_newuserdatauv(lua, sizeof(*value) + length + 1, 1);
    char *storage = (char *)(value + 1);
    memcpy(storage, player->animation->name, length + 1);
    *value = (gdim2__AnimatedSpriteValue) {
        .asset = asset,
        .name = storage,
        .current_frame = player->current_frame,
        .time = player->time
    };
    luaL_setmetatable(lua, GDIM2_ANIMATED_VALUE_META);
    lua_pushvalue(lua, binding_index);
    lua_setiuservalue(lua, -2, 1);
}

/* Rebuilds a player for a standalone value; the stored frame index is kept
 * when the reloaded animation still has it. */
static gatlas_AnimationPlayer gdim2__dim2_value_player(lua_State *lua, gdim2__AnimatedSpriteValue *value) {
    const gatlas_Animation *animation = gatlas_animation(&value->asset->atlas, value->name);
    if (!animation) {
        luaL_error(lua, "atlas '%s' no longer has animation '%s'", value->asset->path, value->name);
    }
    gatlas_AnimationPlayer player = gatlas_AnimationPlayer_new(&value->asset->atlas, animation);
    if (value->current_frame < player.animation->count) {
        player.current_frame = value->current_frame;
    }
    player.time = value->time;
    return player;
}

static int gdim2__dim2_atlas_load(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    const char *path = luaL_checkstring(lua, 1);
    gdim2__AtlasAsset *asset = gdim2__dim2_load_asset(ctx->alloc, ctx->dim2, path);
    if (!asset) {
        return luaL_error(lua, "could not load atlas '%s'", path);
    }
    gdim2__AtlasValue *value = lua_newuserdatauv(lua, sizeof(*value), 1);
    *value = (gdim2__AtlasValue) {.asset = asset};
    luaL_setmetatable(lua, GDIM2_ATLAS_META);
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int gdim2__dim2_atlas_sprite(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gdim2__AtlasValue *atlas = luaL_checkudata(lua, 1, GDIM2_ATLAS_META);
    const char *name = luaL_checkstring(lua, 2);
    gdim2__dim2_check_asset(lua, ctx, atlas->asset);
    if (!gatlas_frame(&atlas->asset->atlas, name)) {
        return luaL_error(lua, "atlas '%s' has no sprite '%s'", atlas->asset->path, name);
    }
    gdim2__dim2_push_sprite_value(lua, atlas->asset, name, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_atlas_animation(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gdim2__AtlasValue *atlas = luaL_checkudata(lua, 1, GDIM2_ATLAS_META);
    const char *name = luaL_checkstring(lua, 2);
    gdim2__dim2_check_asset(lua, ctx, atlas->asset);
    const gatlas_Animation *animation = gatlas_animation(&atlas->asset->atlas, name);
    if (!animation) {
        return luaL_error(lua, "atlas '%s' has no animation '%s'", atlas->asset->path, name);
    }
    gatlas_AnimationPlayer player = gatlas_AnimationPlayer_new(&atlas->asset->atlas, animation);
    gdim2__dim2_push_animated_value(lua, atlas->asset, &player, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_sprite_load(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    const char *path = luaL_checkstring(lua, 1);
    const char *name = luaL_checkstring(lua, 2);
    gdim2__AtlasAsset *asset = gdim2__dim2_load_asset(ctx->alloc, ctx->dim2, path);
    const gatlas_Frame *frame = asset ? gatlas_frame(&asset->atlas, name) : NULL;
    if (!frame) {
        return luaL_error(lua, "could not load sprite '%s' from atlas '%s'", name, path);
    }
    gdim2__dim2_push_sprite_value(lua, asset, name, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_animated_load(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    const char *path = luaL_checkstring(lua, 1);
    const char *name = luaL_checkstring(lua, 2);
    gdim2__AtlasAsset *asset = gdim2__dim2_load_asset(ctx->alloc, ctx->dim2, path);
    const gatlas_Animation *animation = asset ? gatlas_animation(&asset->atlas, name) : NULL;
    if (!animation) {
        return luaL_error(lua, "could not load animation '%s' from atlas '%s'", name, path);
    }
    gatlas_AnimationPlayer player = gatlas_AnimationPlayer_new(&asset->atlas, animation);
    gdim2__dim2_push_animated_value(lua, asset, &player, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_sprite_set(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    const char *name = NULL;
    const char *path = NULL;
    gdim2__AtlasAsset *asset = NULL;
    gdim2__SpriteValue *value = luaL_testudata(lua, 2, GDIM2_SPRITE_VALUE_META);
    if (value) {
        gdim2__dim2_check_asset(lua, ctx, value->asset);
        asset = value->asset;
        name = value->name;
    } else {
        path = luaL_checkstring(lua, 2);
        name = luaL_checkstring(lua, 3);
        asset = gdim2__dim2_load_asset(ctx->alloc, ctx->dim2, path);
    }
    const gatlas_Frame *frame = asset ? gatlas_frame(&asset->atlas, name) : NULL;
    if (!frame) {
        return luaL_error(lua, "could not load sprite '%s' from atlas '%s'", name, path ? path : asset->path);
    }
    gdim2__Sprite sprite = {asset, frame};
    if (!gecs_Component_set(ctx->alloc, ctx->dim2->world, entity, ctx->dim2->sprite, &sprite)) {
        return luaL_error(lua, "could not allocate Sprite component");
    }
    return 0;
}

static int gdim2__dim2_sprite_get(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    gdim2__Sprite *sprite = gecs_Component_get(ctx->dim2->world, entity, ctx->dim2->sprite);
    if (!sprite) {
        return luaL_error(lua, "Entity does not have a Sprite component");
    }
    gdim2__dim2_push_sprite_value(lua, sprite->asset, sprite->frame->name, lua_upvalueindex(1));
    return 1;
}

static int gdim2__dim2_animated_set(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    gdim2__AtlasAsset *asset = NULL;
    const char *name = NULL;
    const char *path = NULL;
    gdim2__AnimatedSpriteValue *value = luaL_testudata(lua, 2, GDIM2_ANIMATED_VALUE_META);
    if (value) {
        gdim2__dim2_check_asset(lua, ctx, value->asset);
        asset = value->asset;
        name = value->name;
    } else {
        path = luaL_checkstring(lua, 2);
        name = luaL_checkstring(lua, 3);
        asset = gdim2__dim2_load_asset(ctx->alloc, ctx->dim2, path);
    }
    const gatlas_Animation *animation = asset ? gatlas_animation(&asset->atlas, name) : NULL;
    if (!animation) {
        return luaL_error(lua, "could not load animation '%s' from atlas '%s'", name, path ? path : asset->path);
    }
    gdim2__AnimatedSprite animated = {asset, gatlas_AnimationPlayer_new(&asset->atlas, animation)};
    if (value) {
        animated.player = gdim2__dim2_value_player(lua, value);
    }
    if (!gecs_Component_set(ctx->alloc, ctx->dim2->world, entity, ctx->dim2->animated, &animated)) {
        return luaL_error(lua, "could not allocate AnimatedSprite component");
    }
    return 0;
}

static int gdim2__dim2_animated_get(lua_State *lua) {
    gdim2__Dim2Bindings *ctx = gdim2__dim2_bindings(lua);
    gecs_Entity entity = gdim2__dim2_check_entity(lua, ctx, 1);
    gdim2__AnimatedSprite *animated = gecs_Component_get(ctx->dim2->world, entity, ctx->dim2->animated);
    if (!animated) {
        return luaL_error(lua, "Entity does not have an AnimatedSprite component");
    }
    gdim2__dim2_push_animated_value(lua, animated->asset, &animated->player, lua_upvalueindex(1));
    return 1;
}

static void gdim2__dim2_set_function(galloc_Allocator alloc, lua_State *lua, int context, const char *name, lua_CFunction fn) {
    (void)alloc;
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, fn, 1);
    lua_setfield(lua, -2, name);
}

bool gdim2_bind_lua(galloc_Allocator alloc, gdim2_Dim2 *dim2, lua_State *lua) {
    if (!dim2 || !lua) {
        return false;
    }
    if (dim2->gdim2__dim2_bindings->references == SIZE_MAX) {
        return false;
    }
    luaL_newmetatable(lua, GDIM2_DIM2_BINDINGS_META);
    lua_pushcfunction(lua, gdim2__dim2_bindings_gc);
    lua_setfield(lua, -2, "__gc");
    lua_pop(lua, 1);
    gdim2__Dim2Bindings **handle = lua_newuserdatauv(lua, sizeof(*handle), 0);
    *handle = NULL;
    luaL_setmetatable(lua, GDIM2_DIM2_BINDINGS_META);
    *handle = dim2->gdim2__dim2_bindings;
    (*handle)->references++;
    int context = lua_gettop(lua);
    luaL_newmetatable(lua, GDIM2_TRANSFORM_META);
    gdim2__dim2_set_function(alloc, lua, context, "__index", gdim2__dim2_transform_index);
    gdim2__dim2_set_function(alloc, lua, context, "__newindex", gdim2__dim2_transform_newindex);
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_TRANSFORM_VEC_META);
    gtypes_install_vec2_ops(alloc, lua, -1);
    gdim2__dim2_set_function(alloc, lua, context, "__index", gdim2__dim2_vector_index);
    gdim2__dim2_set_function(alloc, lua, context, "__newindex", gdim2__dim2_vector_newindex);
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_ATLAS_META);
    lua_newtable(lua);
    gdim2__dim2_set_function(alloc, lua, context, "sprite", gdim2__dim2_atlas_sprite);
    gdim2__dim2_set_function(alloc, lua, context, "animation", gdim2__dim2_atlas_animation);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_SPRITE_VALUE_META);
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_ANIMATED_VALUE_META);
    lua_pop(lua, 1);
    lua_newtable(lua);
    gdim2__dim2_set_function(alloc, lua, context, "new", gdim2__dim2_transform_new);
    gdim2__dim2_set_function(alloc, lua, context, "set", gdim2__dim2_transform_set);
    gdim2__dim2_set_function(alloc, lua, context, "get", gdim2__dim2_transform_get);
    gdim2__dim2_set_function(alloc, lua, context, "has", gdim2__dim2_transform_has);
    gdim2__dim2_set_function(alloc, lua, context, "remove", gdim2__dim2_transform_remove);
    lua_setglobal(lua, "Transform2D");
    lua_newtable(lua);
    gdim2__dim2_set_function(alloc, lua, context, "load", gdim2__dim2_atlas_load);
    lua_setglobal(lua, "Atlas");
    lua_newtable(lua);
    gdim2__dim2_set_function(alloc, lua, context, "load", gdim2__dim2_sprite_load);
    gdim2__dim2_set_function(alloc, lua, context, "set", gdim2__dim2_sprite_set);
    gdim2__dim2_set_function(alloc, lua, context, "get", gdim2__dim2_sprite_get);
    gdim2__dim2_set_function(alloc, lua, context, "has", gdim2__dim2_sprite_has);
    gdim2__dim2_set_function(alloc, lua, context, "remove", gdim2__dim2_sprite_remove);
    lua_setglobal(lua, "Sprite");
    lua_newtable(lua);
    gdim2__dim2_set_function(alloc, lua, context, "load", gdim2__dim2_animated_load);
    gdim2__dim2_set_function(alloc, lua, context, "set", gdim2__dim2_animated_set);
    gdim2__dim2_set_function(alloc, lua, context, "get", gdim2__dim2_animated_get);
    gdim2__dim2_set_function(alloc, lua, context, "has", gdim2__dim2_animated_has);
    gdim2__dim2_set_function(alloc, lua, context, "remove", gdim2__dim2_animated_remove);
    lua_setglobal(lua, "AnimatedSprite");
    lua_pop(lua, 1);
    return true;
}

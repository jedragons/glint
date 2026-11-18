#include "./bindings.h"
#include "./dim2_internal.h"

#include <glint/modules/script/bindings.h>
#include <glint/modules/types/bindings.h>

#include <lauxlib.h>
#include <lua.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

struct gdim2_bindings_State {
    galloc_Allocator alloc;
    gdim2_Dim2* dim2;
    size_t references;
};

#define GDIM2_BINDINGS_META "glint.Dim2Bindings"
#define GDIM2_BINDINGS_TRANSFORM_META "glint.Transform2D"
#define GDIM2_BINDINGS_TRANSFORM_VEC_META "glint.Transform2D.Vector"
#define GDIM2_BINDINGS_ATLAS_META "glint.Atlas"
#define GDIM2_BINDINGS_SPRITE_VALUE_META "glint.SpriteValue"
#define GDIM2_BINDINGS_ANIMATED_VALUE_META "glint.AnimatedSpriteValue"

typedef struct gdim2_bindings__TransformRef {
    gdim2_bindings_State* bindings;
    gdim2_Transform2D* storage;
    gecs_ID entity;
    int32_t vector_field;
} gdim2_bindings__TransformRef;

typedef struct gdim2_bindings__AtlasValue {
    gdim2__AtlasAsset* asset;
} gdim2_bindings__AtlasValue;

// Standalone sprite values remember the frame name and re-resolve it on use,
// so they survive atlas reloads that preserve the name.
typedef struct gdim2_bindings__SpriteValue {
    gdim2__AtlasAsset* asset;

    // Points into bytes stored after this struct.
    const char* name;
} gdim2_bindings__SpriteValue;

typedef struct gdim2_bindings__AnimatedSpriteValue {
    gdim2__AtlasAsset* asset;
    const char* name; // Points into bytes stored after this struct.
    size_t current_frame;
    float time;
} gdim2_bindings__AnimatedSpriteValue;

typedef struct gdim2_bindings__BindArgs {
    galloc_Allocator alloc;
    gdim2_Dim2* dim2;
} gdim2_bindings__BindArgs;

static int gdim2_bindings__install(lua_State* lua);
static void gdim2_bindings__release_bindings(gdim2_bindings_State* bindings);
static int gdim2_bindings__bindings_gc(lua_State* lua);
static gdim2_bindings_State* gdim2_bindings__bindings(lua_State* lua);
static gecs_ID gdim2_bindings__check_entity(lua_State* lua, gdim2_bindings_State* ctx, int index);
static gdim2_Transform2D* gdim2_bindings__get_transform(lua_State* lua, gdim2_bindings_State* ctx, gecs_ID entity);
static gvec2_Vec2* gdim2_bindings__transform_vector(gdim2_Transform2D* transform, int32_t field);
static gdim2_Transform2D* gdim2_bindings__ref_transform(
    lua_State* lua,
    gdim2_bindings_State* ctx,
    gdim2_bindings__TransformRef* ref
);
static gvec2_Vec2 gdim2_bindings__check_vec2(lua_State* lua, gdim2_bindings_State* ctx, int index);
static void gdim2_bindings__push_ref(
    lua_State* lua,
    gdim2_bindings_State* bindings,
    gecs_ID entity,
    gdim2_Transform2D* storage,
    int32_t field,
    const char* metatable,
    int binding_index
);
static gdim2_bindings_State* gdim2_bindings__ref_bindings(lua_State* lua, int index);
static int gdim2_bindings__vector_index(lua_State* lua);
static int gdim2_bindings__vector_newindex(lua_State* lua);
static int gdim2_bindings__transform_index(lua_State* lua);
static int gdim2_bindings__transform_newindex(lua_State* lua);
static int gdim2_bindings__transform_new(lua_State* lua);
static int gdim2_bindings__transform_set(lua_State* lua);
static int gdim2_bindings__transform_get(lua_State* lua);
static int gdim2_bindings__component_has(lua_State* lua, gecs_Component component);
static int gdim2_bindings__component_remove(lua_State* lua, gecs_Component component);
static int gdim2_bindings__transform_has(lua_State* lua);
static int gdim2_bindings__transform_remove(lua_State* lua);
static int gdim2_bindings__sprite_has(lua_State* lua);
static int gdim2_bindings__sprite_remove(lua_State* lua);
static int gdim2_bindings__animated_has(lua_State* lua);
static int gdim2_bindings__animated_remove(lua_State* lua);
static void gdim2_bindings__check_asset(lua_State* lua, gdim2_bindings_State* ctx, gdim2__AtlasAsset* asset);
static void gdim2_bindings__push_sprite_value(
    lua_State* lua,
    gdim2__AtlasAsset* asset,
    const char* name,
    int binding_index
);
static void gdim2_bindings__push_animated_value(
    lua_State* lua,
    gdim2__AtlasAsset* asset,
    const gdim2_atlas_AnimationPlayer* player,
    int binding_index
);
static gdim2_atlas_AnimationPlayer gdim2_bindings__value_player(
    lua_State* lua,
    gdim2_bindings__AnimatedSpriteValue* value
);
static int gdim2_bindings__atlas_load(lua_State* lua);
static int gdim2_bindings__atlas_sprite(lua_State* lua);
static int gdim2_bindings__atlas_animation(lua_State* lua);
static int gdim2_bindings__sprite_load(lua_State* lua);
static int gdim2_bindings__animated_load(lua_State* lua);
static int gdim2_bindings__sprite_set(lua_State* lua);
static int gdim2_bindings__sprite_get(lua_State* lua);
static int gdim2_bindings__animated_set(lua_State* lua);
static int gdim2_bindings__animated_get(lua_State* lua);
static void gdim2_bindings__set_function(
    galloc_Allocator alloc,
    lua_State* lua,
    int context,
    const char* name,
    lua_CFunction fn
);

bool gdim2_bindings_bind(galloc_Allocator alloc, gdim2_Dim2* dim2, lua_State* lua) {
    if (dim2 == NULL || lua == NULL) {
        return false;
    }

    if (dim2->bindings->references == SIZE_MAX) {
        return false;
    }

    luaL_newmetatable(lua, GDIM2_BINDINGS_META);
    lua_pushcfunction(lua, gdim2_bindings__bindings_gc);
    lua_setfield(lua, -2, "__gc");
    lua_pop(lua, 1);
    gdim2_bindings_State** handle = lua_newuserdatauv(lua, sizeof(*handle), 0);
    *handle = NULL;
    luaL_setmetatable(lua, GDIM2_BINDINGS_META);
    *handle = dim2->bindings;
    (*handle)->references += 1;
    int context = lua_gettop(lua);
    luaL_newmetatable(lua, GDIM2_BINDINGS_TRANSFORM_META);
    gdim2_bindings__set_function(alloc, lua, context, "__index", gdim2_bindings__transform_index);
    gdim2_bindings__set_function(alloc, lua, context, "__newindex", gdim2_bindings__transform_newindex);
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_BINDINGS_TRANSFORM_VEC_META);
    gtypes_bindings_install_vec2_ops(alloc, lua, -1);
    gdim2_bindings__set_function(alloc, lua, context, "__index", gdim2_bindings__vector_index);
    gdim2_bindings__set_function(alloc, lua, context, "__newindex", gdim2_bindings__vector_newindex);
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_BINDINGS_ATLAS_META);
    lua_newtable(lua);
    gdim2_bindings__set_function(alloc, lua, context, "sprite", gdim2_bindings__atlas_sprite);
    gdim2_bindings__set_function(alloc, lua, context, "animation", gdim2_bindings__atlas_animation);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_BINDINGS_SPRITE_VALUE_META);
    lua_pop(lua, 1);
    luaL_newmetatable(lua, GDIM2_BINDINGS_ANIMATED_VALUE_META);
    lua_pop(lua, 1);
    lua_newtable(lua);
    gdim2_bindings__set_function(alloc, lua, context, "new", gdim2_bindings__transform_new);
    gdim2_bindings__set_function(alloc, lua, context, "set", gdim2_bindings__transform_set);
    gdim2_bindings__set_function(alloc, lua, context, "get", gdim2_bindings__transform_get);
    gdim2_bindings__set_function(alloc, lua, context, "has", gdim2_bindings__transform_has);
    gdim2_bindings__set_function(alloc, lua, context, "remove", gdim2_bindings__transform_remove);
    lua_setglobal(lua, "Transform2D");

    lua_newtable(lua);
    gdim2_bindings__set_function(alloc, lua, context, "load", gdim2_bindings__atlas_load);
    lua_setglobal(lua, "Atlas");

    lua_newtable(lua);
    gdim2_bindings__set_function(alloc, lua, context, "load", gdim2_bindings__sprite_load);
    gdim2_bindings__set_function(alloc, lua, context, "set", gdim2_bindings__sprite_set);
    gdim2_bindings__set_function(alloc, lua, context, "get", gdim2_bindings__sprite_get);
    gdim2_bindings__set_function(alloc, lua, context, "has", gdim2_bindings__sprite_has);
    gdim2_bindings__set_function(alloc, lua, context, "remove", gdim2_bindings__sprite_remove);
    lua_setglobal(lua, "Sprite");

    lua_newtable(lua);
    gdim2_bindings__set_function(alloc, lua, context, "load", gdim2_bindings__animated_load);
    gdim2_bindings__set_function(alloc, lua, context, "set", gdim2_bindings__animated_set);
    gdim2_bindings__set_function(alloc, lua, context, "get", gdim2_bindings__animated_get);
    gdim2_bindings__set_function(alloc, lua, context, "has", gdim2_bindings__animated_has);
    gdim2_bindings__set_function(alloc, lua, context, "remove", gdim2_bindings__animated_remove);
    lua_setglobal(lua, "AnimatedSprite");
    lua_pop(lua, 1);
    return true;
}

bool gdim2_bindings_install(galloc_Allocator alloc, gdim2_Dim2* dim2, lua_State* lua) {
    if (dim2 == NULL || lua == NULL) {
        return false;
    }

    gdim2_bindings__BindArgs args = {.alloc = alloc, .dim2 = dim2};
    lua_pushcfunction(lua, gdim2_bindings__install);
    lua_pushlightuserdata(lua, &args);
    return lua_pcall(lua, 1, 0, 0) == LUA_OK;
}

gdim2_bindings_State* gdim2_bindings__new(galloc_Allocator alloc, gdim2_Dim2* dim2) {
    gdim2_bindings_State* bindings = galloc_calloc(alloc, 1, sizeof(*bindings));
    if (bindings != NULL) {
        *bindings = (gdim2_bindings_State) {.alloc = alloc, .dim2 = dim2, .references = 1};
    }

    return bindings;
}

void gdim2_bindings__destroy(gdim2_bindings_State* bindings) {
    bindings->dim2 = NULL;
    gdim2_bindings__release_bindings(bindings);
}

static int gdim2_bindings__install(lua_State* lua) {
    gdim2_bindings__BindArgs* args = lua_touserdata(lua, 1);
    if (!gdim2_bindings_bind(args->alloc, args->dim2, lua)) {
        return luaL_error(lua, "could not register 2D bindings");
    }

    return 0;
}

static void gdim2_bindings__release_bindings(gdim2_bindings_State* bindings) {
    bindings->references -= 1;
    if (bindings->references == 0) {
        galloc_free(bindings->alloc, bindings);
    }
}

static int gdim2_bindings__bindings_gc(lua_State* lua) {
    gdim2_bindings_State** handle = luaL_checkudata(lua, 1, GDIM2_BINDINGS_META);
    if (*handle != NULL) {
        gdim2_bindings__release_bindings(*handle);
        *handle = NULL;
    }

    return 0;
}

static gdim2_bindings_State* gdim2_bindings__bindings(lua_State* lua) {
    gdim2_bindings_State** handle = lua_touserdata(lua, lua_upvalueindex(1));
    bool bindings_alive = handle != NULL && *handle != NULL && (*handle)->dim2 != NULL;
    if (!bindings_alive) {
        luaL_error(lua, "2D module is no longer available");
    }

    return *handle;
}

static gecs_ID gdim2_bindings__check_entity(lua_State* lua, gdim2_bindings_State* ctx, int index) {
    gecs_Entity handle = gscript_bindings_check_entity(lua, index);
    if (handle.world != ctx->dim2->world || !gecs_Entity_alive(handle)) {
        luaL_error(lua, "Entity is invalid or belongs to a different world");
    }

    return handle.id;
}

static gdim2_Transform2D* gdim2_bindings__get_transform(lua_State* lua, gdim2_bindings_State* ctx, gecs_ID entity) {
    if (!gecs_Entity_alive((gecs_Entity) {.world = ctx->dim2->world, .id = entity})) {
        luaL_error(lua, "Entity is no longer valid");
    }
    gdim2_Transform2D* transform =
        gecs_Component_get((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, ctx->dim2->transform);
    if (transform == NULL) {
        luaL_error(lua, "Transform2D component no longer exists");
    }

    return transform;
}

static gvec2_Vec2* gdim2_bindings__transform_vector(gdim2_Transform2D* transform, int32_t field) {
    if (field == 0) {
        return &transform->position;
    }

    if (field == 1) {
        return &transform->scale;
    }

    return &transform->origin;
}

// Standalone values resolve to their own storage; entity references resolve
// through the ECS and error when the component is gone.
static gdim2_Transform2D* gdim2_bindings__ref_transform(
    lua_State* lua,
    gdim2_bindings_State* ctx,
    gdim2_bindings__TransformRef* ref
) {
    if (ref->storage != NULL) {
        return ref->storage;
    }

    return gdim2_bindings__get_transform(lua, ctx, ref->entity);
}

static gvec2_Vec2 gdim2_bindings__check_vec2(lua_State* lua, gdim2_bindings_State* ctx, int index) {
    gdim2_bindings__TransformRef* ref = luaL_testudata(lua, index, GDIM2_BINDINGS_TRANSFORM_VEC_META);
    if (ref != NULL) {
        if (gdim2_bindings__ref_bindings(lua, index)->dim2 != ctx->dim2) {
            luaL_error(lua, "Vec2 belongs to a different 2D module");
        }

        return *gdim2_bindings__transform_vector(gdim2_bindings__ref_transform(lua, ctx, ref), ref->vector_field);
    }

    luaL_checktype(lua, index, LUA_TTABLE);
    lua_getfield(lua, index, "x");
    float x = (float)luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "y");
    float y = (float)luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    return (gvec2_Vec2) {.x = x, .y = y};
}

static void gdim2_bindings__push_ref(
    lua_State* lua,
    gdim2_bindings_State* bindings,
    gecs_ID entity,
    gdim2_Transform2D* storage,
    int32_t field,
    const char* metatable,
    int binding_index
) {
    gdim2_bindings__TransformRef* ref = lua_newuserdatauv(lua, sizeof(*ref), 1);
    *ref = (gdim2_bindings__TransformRef) {
        .bindings = bindings,
        .storage = storage,
        .entity = entity,
        .vector_field = field
    };
    luaL_setmetatable(lua, metatable);
    // The parent object in binding_index keeps this reference alive; for
    // entity references it is the bindings handle, for values the owner.
    lua_pushvalue(lua, binding_index);
    lua_setiuservalue(lua, -2, 1);
}

// Reference uservalues either hold the bindings handle or a parent transform
// reference, which itself keeps the bindings alive.
static gdim2_bindings_State* gdim2_bindings__ref_bindings(lua_State* lua, int index) {
    lua_getiuservalue(lua, index, 1);
    gdim2_bindings_State* result = NULL;
    gdim2_bindings_State** handle = luaL_testudata(lua, -1, GDIM2_BINDINGS_META);
    if (handle != NULL) {
        result = *handle;
    } else {
        gdim2_bindings__TransformRef* ref = luaL_testudata(lua, -1, GDIM2_BINDINGS_TRANSFORM_META);
        result = ref != NULL ? ref->bindings : NULL;
    }

    lua_pop(lua, 1);
    if (result == NULL || result->dim2 == NULL) {
        luaL_error(lua, "2D module is no longer available");
    }

    return result;
}

static int gdim2_bindings__vector_index(lua_State* lua) {
    gdim2_bindings__TransformRef* ref = luaL_checkudata(lua, 1, GDIM2_BINDINGS_TRANSFORM_VEC_META);
    gdim2_bindings_State* ctx = gdim2_bindings__ref_bindings(lua, 1);
    const char* key = luaL_checkstring(lua, 2);
    gvec2_Vec2 value =
        *gdim2_bindings__transform_vector(gdim2_bindings__ref_transform(lua, ctx, ref), ref->vector_field);
    if (strcmp(key, "x") == 0) {
        lua_pushnumber(lua, value.x);
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(lua, value.y);
    } else {
        luaL_getmetatable(lua, GDIM2_BINDINGS_TRANSFORM_VEC_META);
        lua_getfield(lua, -1, "__vec2_methods");
        lua_getfield(lua, -1, key);
        lua_remove(lua, -2);
        lua_remove(lua, -2);
    }

    return 1;
}

static int gdim2_bindings__vector_newindex(lua_State* lua) {
    gdim2_bindings__TransformRef* ref = luaL_checkudata(lua, 1, GDIM2_BINDINGS_TRANSFORM_VEC_META);
    gdim2_bindings_State* ctx = gdim2_bindings__ref_bindings(lua, 1);
    const char* key = luaL_checkstring(lua, 2);
    float value = (float)luaL_checknumber(lua, 3);
    gvec2_Vec2* vector =
        gdim2_bindings__transform_vector(gdim2_bindings__ref_transform(lua, ctx, ref), ref->vector_field);
    if (strcmp(key, "x") == 0) {
        vector->x = value;
    } else if (strcmp(key, "y") == 0) {
        vector->y = value;
    } else {
        return luaL_error(lua, "unknown Vec2 field '%s'", key);
    }

    return 0;
}

static int gdim2_bindings__transform_index(lua_State* lua) {
    gdim2_bindings__TransformRef* ref = luaL_checkudata(lua, 1, GDIM2_BINDINGS_TRANSFORM_META);
    gdim2_bindings_State* ctx = gdim2_bindings__ref_bindings(lua, 1);
    const char* key = luaL_checkstring(lua, 2);
    gdim2_Transform2D* transform = gdim2_bindings__ref_transform(lua, ctx, ref);
    if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(lua, transform->rotation);
    } else if (strcmp(key, "position") == 0 || strcmp(key, "scale") == 0 || strcmp(key, "origin") == 0) {
        int32_t field = strcmp(key, "position") == 0 ? 0 : strcmp(key, "scale") == 0 ? 1 : 2;
        gdim2_bindings__push_ref(lua, ctx, ref->entity, ref->storage, field, GDIM2_BINDINGS_TRANSFORM_VEC_META, 1);
    } else {
        lua_pushnil(lua);
    }

    return 1;
}

static int gdim2_bindings__transform_newindex(lua_State* lua) {
    gdim2_bindings__TransformRef* ref = luaL_checkudata(lua, 1, GDIM2_BINDINGS_TRANSFORM_META);
    gdim2_bindings_State* ctx = gdim2_bindings__ref_bindings(lua, 1);
    const char* key = luaL_checkstring(lua, 2);
    if (strcmp(key, "rotation") == 0) {
        float value = (float)luaL_checknumber(lua, 3);
        gdim2_bindings__ref_transform(lua, ctx, ref)->rotation = value;
    } else if (strcmp(key, "position") == 0 || strcmp(key, "scale") == 0 || strcmp(key, "origin") == 0) {
        gvec2_Vec2 value = gdim2_bindings__check_vec2(lua, ctx, 3);
        int32_t field = strcmp(key, "position") == 0 ? 0 : strcmp(key, "scale") == 0 ? 1 : 2;
        *gdim2_bindings__transform_vector(gdim2_bindings__ref_transform(lua, ctx, ref), field) = value;
    } else {
        return luaL_error(lua, "unknown Transform2D field '%s'", key);
    }

    return 0;
}

// Optional arguments: position, scale, origin, rotation.
static int gdim2_bindings__transform_new(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    int top = lua_gettop(lua);
    if (top != 0 && top != 4) {
        return luaL_error(lua, "Transform2D.new expects no arguments or position, scale, origin, rotation");
    }

    gdim2_bindings__TransformRef* ref = lua_newuserdatauv(lua, sizeof(*ref) + sizeof(gdim2_Transform2D), 1);
    gdim2_Transform2D* storage = (gdim2_Transform2D*)(ref + 1);
    *storage = (gdim2_Transform2D) {.scale = {1, 1}};
    *ref = (gdim2_bindings__TransformRef) {.bindings = ctx, .storage = storage};
    luaL_setmetatable(lua, GDIM2_BINDINGS_TRANSFORM_META);
    if (top != 0) {
        storage->position = gdim2_bindings__check_vec2(lua, ctx, 1);
        storage->scale = gdim2_bindings__check_vec2(lua, ctx, 2);
        storage->origin = gdim2_bindings__check_vec2(lua, ctx, 3);
        storage->rotation = (float)luaL_checknumber(lua, 4);
    }

    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int gdim2_bindings__transform_set(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    gdim2_Transform2D transform = {.scale = {1, 1}};
    gdim2_bindings__TransformRef* ref = luaL_testudata(lua, 2, GDIM2_BINDINGS_TRANSFORM_META);
    if (ref != NULL) {
        if (gdim2_bindings__ref_bindings(lua, 2)->dim2 != ctx->dim2) {
            return luaL_error(lua, "Transform2D belongs to a different 2D module");
        }

        transform = *gdim2_bindings__ref_transform(lua, ctx, ref);
    } else if (lua_isnoneornil(lua, 2) == 0) {
        transform.position = gdim2_bindings__check_vec2(lua, ctx, 2);
        transform.scale = gdim2_bindings__check_vec2(lua, ctx, 3);
        transform.origin = gdim2_bindings__check_vec2(lua, ctx, 4);
        transform.rotation = (float)luaL_checknumber(lua, 5);
    }

    if (gecs_Component_set((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, ctx->dim2->transform, &transform)
        == NULL) {
        return luaL_error(lua, "could not allocate Transform2D component");
    }

    gdim2_bindings__push_ref(lua, ctx, entity, NULL, 0, GDIM2_BINDINGS_TRANSFORM_META, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__transform_get(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    (void)gdim2_bindings__get_transform(lua, ctx, entity);
    gdim2_bindings__push_ref(lua, ctx, entity, NULL, 0, GDIM2_BINDINGS_TRANSFORM_META, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__component_has(lua_State* lua, gecs_Component component) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    lua_pushboolean(
        lua,
        gecs_Component_get((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, component) != NULL
    );
    return 1;
}

static int gdim2_bindings__component_remove(lua_State* lua, gecs_Component component) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    gecs_Component_remove((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, component);
    return 0;
}

static int gdim2_bindings__transform_has(lua_State* lua) {
    return gdim2_bindings__component_has(lua, gdim2_bindings__bindings(lua)->dim2->transform);
}

static int gdim2_bindings__transform_remove(lua_State* lua) {
    return gdim2_bindings__component_remove(lua, gdim2_bindings__bindings(lua)->dim2->transform);
}

static int gdim2_bindings__sprite_has(lua_State* lua) {
    return gdim2_bindings__component_has(lua, gdim2_bindings__bindings(lua)->dim2->sprite);
}

static int gdim2_bindings__sprite_remove(lua_State* lua) {
    return gdim2_bindings__component_remove(lua, gdim2_bindings__bindings(lua)->dim2->sprite);
}

static int gdim2_bindings__animated_has(lua_State* lua) {
    return gdim2_bindings__component_has(lua, gdim2_bindings__bindings(lua)->dim2->animated);
}

static int gdim2_bindings__animated_remove(lua_State* lua) {
    return gdim2_bindings__component_remove(lua, gdim2_bindings__bindings(lua)->dim2->animated);
}

// Standalone values must not outlive the atlas they came from; every use
// re-validates the asset against the module's live list.
static void gdim2_bindings__check_asset(lua_State* lua, gdim2_bindings_State* ctx, gdim2__AtlasAsset* asset) {
    for (gdim2__AtlasAsset* current = ctx->dim2->assets; current != NULL; current = current->next) {
        if (current == asset) {
            return;
        }
    }

    luaL_error(lua, "Atlas is no longer available");
}

static void gdim2_bindings__push_sprite_value(
    lua_State* lua,
    gdim2__AtlasAsset* asset,
    const char* name,
    int binding_index
) {
    size_t length = strlen(name);
    gdim2_bindings__SpriteValue* value = lua_newuserdatauv(lua, sizeof(*value) + length + 1, 1);
    char* storage = (char*)(value + 1);
    memcpy(storage, name, length + 1);
    *value = (gdim2_bindings__SpriteValue) {.asset = asset, .name = storage};
    luaL_setmetatable(lua, GDIM2_BINDINGS_SPRITE_VALUE_META);
    lua_pushvalue(lua, binding_index);
    lua_setiuservalue(lua, -2, 1);
}

static void gdim2_bindings__push_animated_value(
    lua_State* lua,
    gdim2__AtlasAsset* asset,
    const gdim2_atlas_AnimationPlayer* player,
    int binding_index
) {
    size_t length = strlen(player->animation->name);
    gdim2_bindings__AnimatedSpriteValue* value = lua_newuserdatauv(lua, sizeof(*value) + length + 1, 1);
    char* storage = (char*)(value + 1);
    memcpy(storage, player->animation->name, length + 1);
    *value = (gdim2_bindings__AnimatedSpriteValue) {
        .asset = asset,
        .name = storage,
        .current_frame = player->current_frame,
        .time = player->time
    };
    luaL_setmetatable(lua, GDIM2_BINDINGS_ANIMATED_VALUE_META);
    lua_pushvalue(lua, binding_index);
    lua_setiuservalue(lua, -2, 1);
}

// Rebuilds a player for a standalone value; the stored frame index is kept
// when the reloaded animation still has it.
static gdim2_atlas_AnimationPlayer gdim2_bindings__value_player(
    lua_State* lua,
    gdim2_bindings__AnimatedSpriteValue* value
) {
    const gdim2_atlas_Animation* animation = gdim2_atlas_animation(&value->asset->atlas, value->name);
    if (animation == NULL) {
        luaL_error(lua, "atlas '%s' no longer has animation '%s'", value->asset->path, value->name);
    }

    gdim2_atlas_AnimationPlayer player = gdim2_atlas_AnimationPlayer_new(&value->asset->atlas, animation);
    if (value->current_frame < player.animation->count) {
        player.current_frame = value->current_frame;
    }

    player.time = value->time;
    return player;
}

static int gdim2_bindings__atlas_load(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    const char* path = luaL_checkstring(lua, 1);
    gdim2__AtlasAsset* asset = gdim2__load_asset(ctx->alloc, ctx->dim2, path);
    if (asset == NULL) {
        return luaL_error(lua, "could not load atlas '%s'", path);
    }

    gdim2_bindings__AtlasValue* value = lua_newuserdatauv(lua, sizeof(*value), 1);
    *value = (gdim2_bindings__AtlasValue) {.asset = asset};
    luaL_setmetatable(lua, GDIM2_BINDINGS_ATLAS_META);
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int gdim2_bindings__atlas_sprite(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gdim2_bindings__AtlasValue* atlas = luaL_checkudata(lua, 1, GDIM2_BINDINGS_ATLAS_META);
    const char* name = luaL_checkstring(lua, 2);
    gdim2_bindings__check_asset(lua, ctx, atlas->asset);
    if (gdim2_atlas_frame(&atlas->asset->atlas, name) == NULL) {
        return luaL_error(lua, "atlas '%s' has no sprite '%s'", atlas->asset->path, name);
    }

    gdim2_bindings__push_sprite_value(lua, atlas->asset, name, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__atlas_animation(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gdim2_bindings__AtlasValue* atlas = luaL_checkudata(lua, 1, GDIM2_BINDINGS_ATLAS_META);
    const char* name = luaL_checkstring(lua, 2);
    gdim2_bindings__check_asset(lua, ctx, atlas->asset);
    const gdim2_atlas_Animation* animation = gdim2_atlas_animation(&atlas->asset->atlas, name);
    if (animation == NULL) {
        return luaL_error(lua, "atlas '%s' has no animation '%s'", atlas->asset->path, name);
    }

    gdim2_atlas_AnimationPlayer player = gdim2_atlas_AnimationPlayer_new(&atlas->asset->atlas, animation);
    gdim2_bindings__push_animated_value(lua, atlas->asset, &player, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__sprite_load(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    const char* path = luaL_checkstring(lua, 1);
    const char* name = luaL_checkstring(lua, 2);
    gdim2__AtlasAsset* asset = gdim2__load_asset(ctx->alloc, ctx->dim2, path);
    const gdim2_atlas_Frame* frame = asset != NULL ? gdim2_atlas_frame(&asset->atlas, name) : NULL;
    if (frame == NULL) {
        return luaL_error(lua, "could not load sprite '%s' from atlas '%s'", name, path);
    }

    gdim2_bindings__push_sprite_value(lua, asset, name, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__animated_load(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    const char* path = luaL_checkstring(lua, 1);
    const char* name = luaL_checkstring(lua, 2);
    gdim2__AtlasAsset* asset = gdim2__load_asset(ctx->alloc, ctx->dim2, path);
    const gdim2_atlas_Animation* animation = asset != NULL ? gdim2_atlas_animation(&asset->atlas, name) : NULL;
    if (animation == NULL) {
        return luaL_error(lua, "could not load animation '%s' from atlas '%s'", name, path);
    }

    gdim2_atlas_AnimationPlayer player = gdim2_atlas_AnimationPlayer_new(&asset->atlas, animation);
    gdim2_bindings__push_animated_value(lua, asset, &player, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__sprite_set(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    const char* name = NULL;
    const char* path = NULL;
    gdim2__AtlasAsset* asset = NULL;
    gdim2_bindings__SpriteValue* value = luaL_testudata(lua, 2, GDIM2_BINDINGS_SPRITE_VALUE_META);
    if (value != NULL) {
        gdim2_bindings__check_asset(lua, ctx, value->asset);
        asset = value->asset;
        name = value->name;
    } else {
        path = luaL_checkstring(lua, 2);
        name = luaL_checkstring(lua, 3);
        asset = gdim2__load_asset(ctx->alloc, ctx->dim2, path);
    }

    const gdim2_atlas_Frame* frame = asset != NULL ? gdim2_atlas_frame(&asset->atlas, name) : NULL;
    if (frame == NULL) {
        return luaL_error(lua, "could not load sprite '%s' from atlas '%s'", name, path != NULL ? path : asset->path);
    }

    gdim2__Sprite sprite = {asset, frame};
    if (gecs_Component_set((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, ctx->dim2->sprite, &sprite)
        == NULL) {
        return luaL_error(lua, "could not allocate Sprite component");
    }

    return 0;
}

static int gdim2_bindings__sprite_get(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    gdim2__Sprite* sprite =
        gecs_Component_get((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, ctx->dim2->sprite);
    if (sprite == NULL) {
        return luaL_error(lua, "Entity does not have a Sprite component");
    }

    gdim2_bindings__push_sprite_value(lua, sprite->asset, sprite->frame->name, lua_upvalueindex(1));
    return 1;
}

static int gdim2_bindings__animated_set(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    gdim2__AtlasAsset* asset = NULL;
    const char* name = NULL;
    const char* path = NULL;
    gdim2_bindings__AnimatedSpriteValue* value = luaL_testudata(lua, 2, GDIM2_BINDINGS_ANIMATED_VALUE_META);
    if (value != NULL) {
        gdim2_bindings__check_asset(lua, ctx, value->asset);
        asset = value->asset;
        name = value->name;
    } else {
        path = luaL_checkstring(lua, 2);
        name = luaL_checkstring(lua, 3);
        asset = gdim2__load_asset(ctx->alloc, ctx->dim2, path);
    }

    const gdim2_atlas_Animation* animation = asset != NULL ? gdim2_atlas_animation(&asset->atlas, name) : NULL;
    if (animation == NULL) {
        return luaL_error(
            lua,
            "could not load animation '%s' from atlas '%s'",
            name,
            path != NULL ? path : asset->path
        );
    }

    gdim2__AnimatedSprite animated = {asset, gdim2_atlas_AnimationPlayer_new(&asset->atlas, animation)};
    if (value != NULL) {
        animated.player = gdim2_bindings__value_player(lua, value);
    }

    if (gecs_Component_set((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, ctx->dim2->animated, &animated)
        == NULL) {
        return luaL_error(lua, "could not allocate AnimatedSprite component");
    }

    return 0;
}

static int gdim2_bindings__animated_get(lua_State* lua) {
    gdim2_bindings_State* ctx = gdim2_bindings__bindings(lua);
    gecs_ID entity = gdim2_bindings__check_entity(lua, ctx, 1);
    gdim2__AnimatedSprite* animated =
        gecs_Component_get((gecs_Entity) {.world = ctx->dim2->world, .id = entity}, ctx->dim2->animated);
    if (animated == NULL) {
        return luaL_error(lua, "Entity does not have an AnimatedSprite component");
    }

    gdim2_bindings__push_animated_value(lua, animated->asset, &animated->player, lua_upvalueindex(1));
    return 1;
}

static void gdim2_bindings__set_function(
    galloc_Allocator alloc,
    lua_State* lua,
    int context,
    const char* name,
    lua_CFunction fn
) {
    (void)alloc;
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, fn, 1);
    lua_setfield(lua, -2, name);
}

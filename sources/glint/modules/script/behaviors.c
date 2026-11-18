#include "./bindings.h"
#include "script_internal.h"

#include <glint/modules/assets/cache.h>
#include <glint/modules/script/behaviors.h>
#include <glint/platform/logging.h>

#include <lauxlib.h>
#include <lua.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct gscript_Asset {
    int module;
    char* path;
    char* native_path;
    bool dirty;
    bool error_state;
};

typedef struct gscript_behavior__behaviors_Behavior {
    gecs_ID entity;
    gscript_Asset* asset;
    uint64_t version;
    bool destroying;
} gscript_behavior__behaviors_Behavior;

typedef struct gscript_behavior__behaviors_Snapshot {
    gecs_ID entity;
    uint64_t version;
} gscript_behavior__behaviors_Snapshot;

struct gscript_behavior_Behaviors {
    galloc_Allocator alloc;
    gscript_lua_Runtime* script;
    gecs_World* world;
    gvfs_Vfs* vfs;
    gasset_Cache* cache;
    bool reload_failed;
    gscript_behavior__behaviors_Behavior** items;
    gecs_Component component;
    size_t count;
    size_t capacity;
    uint64_t version;
    bool shutting_down;
};

static void gscript_behavior__behaviors_component_destroy(void* value);
static int gscript_behavior__behaviors_create_state(lua_State* lua);
static size_t gscript_behavior__behaviors_find_behavior(const gscript_behavior_Behaviors* behaviors, gecs_ID entity);
static void gscript_behavior__behaviors_remove_behavior(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    size_t index
);
static bool gscript_behavior__behaviors_reserve_behavior(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors);
static char* gscript_behavior__behaviors_behavior_copy_string(galloc_Allocator alloc, const char* text);
static int gscript_behavior__behaviors_behavior_error_protected(lua_State* lua);
static bool gscript_behavior__behaviors_behavior_error(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    const char* message
);
static void gscript_behavior__behaviors_call(
    gscript_behavior_Behaviors* behaviors,
    gscript_Asset* asset,
    const char* callback,
    gecs_ID entity
);
static void gscript_behavior__behaviors_destroy_behavior(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    gecs_ID entity
);
static bool gscript_behavior__behaviors_on_destroy(gecs_Entity handle, void* user);
static void gscript_behavior__behaviors_on_native_destroy(gecs_Entity handle, void* user);
static bool gscript_behavior__behaviors_on_attach(gecs_Entity handle, const char* path, void* user);
static void gscript_behavior__behaviors_reload_error(gscript_behavior_Behaviors* behaviors, gscript_Asset* asset);
static bool gscript_behavior__behaviors_asset_reload(
    galloc_Allocator alloc,
    void* instance,
    const char* path,
    void* user
);
static bool gscript_behavior__behaviors_asset_load(
    galloc_Allocator alloc,
    void* instance,
    const char* path,
    void* user
);
static void gscript_behavior__behaviors_asset_unload(galloc_Allocator alloc, void* instance, void* user);
static gscript_Asset* gscript_behavior__behaviors_asset_get(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    const char* path
);

const char* gscript_asset_path(const gscript_Asset* asset) {
    return asset != NULL ? asset->path : NULL;
}

gscript_behavior_Behaviors* gscript_behavior_new(
    gscript_lua_Runtime* script,
    gecs_World* world,
    gvfs_Vfs* vfs,
    gasset_Cache* cache
) {
    if (script == NULL || world == NULL || vfs == NULL || cache == NULL) {
        return NULL;
    }

    galloc_Allocator alloc = glint_allocator(world);
    gscript_behavior_Behaviors* behaviors = galloc_calloc(alloc, 1, sizeof(*behaviors));
    if (behaviors == NULL) {
        return NULL;
    }

    behaviors->cache = cache;
    if (!gecs_World_observe_destroy(world, gscript_behavior__behaviors_on_native_destroy, behaviors)) {
        galloc_free(alloc, behaviors);
        return NULL;
    }
    behaviors->component = gecs_Component_register(
        world,
        "gscript_behavior_Behavior",
        sizeof(gscript_behavior_Behavior),
        gscript_behavior__behaviors_component_destroy
    );
    if (behaviors->component == 0) {
        gecs_World_unobserve_destroy(world, gscript_behavior__behaviors_on_native_destroy, behaviors);
        galloc_free(alloc, behaviors);
        return NULL;
    }

    behaviors->alloc = alloc;
    behaviors->script = script;
    behaviors->world = world;
    behaviors->vfs = vfs;
    if (!gasset_Cache_register(
            alloc,
            behaviors->cache,
            "lua",
            sizeof(gscript_Asset),
            gscript_behavior__behaviors_asset_load,
            gscript_behavior__behaviors_asset_reload,
            gscript_behavior__behaviors_asset_unload,
            behaviors
        )) {
        gecs_World_unobserve_destroy(world, gscript_behavior__behaviors_on_native_destroy, behaviors);
        galloc_free(alloc, behaviors);
        return NULL;
    }

    gscript_lua_set_entity_destroy(script, gscript_behavior__behaviors_on_destroy, behaviors);
    gscript_lua_set_entity_attach(script, gscript_behavior__behaviors_on_attach, behaviors);
    return behaviors;
}

bool gscript_behavior_attach(gscript_behavior_Behaviors* behaviors, gecs_Entity handle, const char* path) {
    gecs_ID entity = handle.id;
    if (behaviors == NULL) {
        return false;
    }

    galloc_Allocator alloc = behaviors->alloc;
    if (path == NULL || handle.world != behaviors->world || !gecs_Entity_alive(handle)) {
        return gscript_behavior__behaviors_behavior_error(alloc, behaviors, "invalid behavior path or entity");
    }

    if (behaviors->shutting_down) {
        return gscript_behavior__behaviors_behavior_error(alloc, behaviors, "cannot attach a behavior during shutdown");
    }

    size_t index = gscript_behavior__behaviors_find_behavior(behaviors, entity);
    if (index != SIZE_MAX && behaviors->items[index]->destroying) {
        return gscript_behavior__behaviors_behavior_error(
            alloc,
            behaviors,
            "cannot attach a behavior during its destroy callback"
        );
    }

    gscript_Asset* asset = gscript_behavior__behaviors_asset_get(alloc, behaviors, path);
    if (asset == NULL) {
        return false;
    }

    gscript_behavior__behaviors_destroy_behavior(alloc, behaviors, entity);

    if (!gecs_Entity_alive(handle) || !gscript_behavior__behaviors_reserve_behavior(alloc, behaviors)) {
        return gscript_behavior__behaviors_behavior_error(
            alloc,
            behaviors,
            "entity was destroyed or behavior allocation failed"
        );
    }

    gscript_behavior__behaviors_Behavior* item = galloc_calloc(alloc, 1, sizeof(*item));
    if (item == NULL) {
        return gscript_behavior__behaviors_behavior_error(alloc, behaviors, "behavior allocation failed");
    }

    behaviors->version += 1;
    uint64_t version = behaviors->version;
    *item = (gscript_behavior__behaviors_Behavior) {.entity = entity, .asset = asset, .version = version};
    lua_State* lua = gscript_lua_state(behaviors->script);
    lua_pushcfunction(lua, gscript_behavior__behaviors_create_state);
    if (lua_pcall(lua, 0, 1, 0) != LUA_OK) {
        galloc_free(alloc, item);
        return false;
    }

    gscript_behavior_Behavior component = {.asset = asset, .lua = lua, .state_ref = (int)lua_tointeger(lua, -1)};
    lua_pop(lua, 1);
    if (gecs_Component_set(handle, behaviors->component, &component) == NULL) {
        luaL_unref(lua, LUA_REGISTRYINDEX, component.state_ref);
        galloc_free(alloc, item);
        return gscript_behavior__behaviors_behavior_error(alloc, behaviors, "behavior component allocation failed");
    }

    gscript_bindings_forget_entity(behaviors->script, handle);
    behaviors->items[behaviors->count] = item;
    behaviors->count += 1;
    gscript_behavior__behaviors_call(behaviors, asset, "setup", entity);
    return true;
}

bool gscript_behavior_update(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors) {
    return gscript_behavior__update(alloc, behaviors, true);
}

bool gscript_behavior_destroy(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors) {
    if (behaviors == NULL) {
        return true;
    }

    if (behaviors->shutting_down || gasset_Cache_busy(behaviors->cache)) {
        return false;
    }

    behaviors->shutting_down = true;
    while (behaviors->count > 0) {
        gecs_ID entity = behaviors->items[behaviors->count - 1]->entity;
        if (gecs_Entity_alive((gecs_Entity) {.world = behaviors->world, .id = entity})) {
            gscript_behavior__behaviors_destroy_behavior(alloc, behaviors, entity);
        }

        size_t index = gscript_behavior__behaviors_find_behavior(behaviors, entity);
        if (index != SIZE_MAX) {
            gscript_behavior__behaviors_remove_behavior(alloc, behaviors, index);
        }

        gscript_bindings_forget_entity(behaviors->script, (gecs_Entity) {.world = behaviors->world, .id = entity});
    }

    gscript_lua_set_entity_attach(behaviors->script, NULL, NULL);
    gscript_lua_set_entity_destroy(behaviors->script, NULL, NULL);
    gecs_World_unobserve_destroy(behaviors->world, gscript_behavior__behaviors_on_native_destroy, behaviors);
    if (!gasset_Cache_unregister(alloc, behaviors->cache, "lua")) {
        behaviors->shutting_down = false;
        return false;
    }

    galloc_free(alloc, behaviors->items);
    galloc_free(alloc, behaviors);
    return true;
}

bool gscript_behavior__update(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors, bool poll) {
    if (behaviors == NULL) {
        return false;
    }

    if (behaviors->reload_failed) {
        behaviors->reload_failed = false;
        return false;
    }

    if (poll) {
        gasset_Cache_poll(alloc, behaviors->cache);
    }

    if (behaviors->reload_failed) {
        behaviors->reload_failed = false;
        return false;
    }

    for (size_t i = 0; i < behaviors->count; i++) {
        gscript_Asset* asset = behaviors->items[i]->asset;
        if (asset->dirty && !gasset_Cache_reload(alloc, behaviors->cache, "lua", asset->path)) {
            behaviors->reload_failed = false;
            if (asset->dirty) {
                return false;
            }
        }
    }

    size_t count = behaviors->count;
    if (count == 0) {
        return true;
    }

    if (count > SIZE_MAX / sizeof(gscript_behavior__behaviors_Snapshot)) {
        return gscript_behavior__behaviors_behavior_error(alloc, behaviors, "too many behaviors");
    }

    gscript_behavior__behaviors_Snapshot* entities = galloc_alloc(alloc, count * sizeof(*entities));
    if (entities == NULL) {
        return gscript_behavior__behaviors_behavior_error(alloc, behaviors, "could not allocate behavior update list");
    }

    for (size_t i = 0; i < count; i++) {
        entities[i] = (gscript_behavior__behaviors_Snapshot) {
            .entity = behaviors->items[i]->entity,
            .version = behaviors->items[i]->version
        };
    }

    for (size_t i = 0; i < count; i++) {
        size_t index = gscript_behavior__behaviors_find_behavior(behaviors, entities[i].entity);
        bool snapshot_is_current = index != SIZE_MAX && behaviors->items[index]->version == entities[i].version;
        if (!snapshot_is_current) {
            continue;
        }

        if (!gecs_Entity_alive((gecs_Entity) {.world = behaviors->world, .id = entities[i].entity})) {
            gscript_bindings_forget_entity(
                behaviors->script,
                (gecs_Entity) {.world = behaviors->world, .id = entities[i].entity}
            );
            gscript_behavior__behaviors_remove_behavior(alloc, behaviors, index);
            continue;
        }

        if (!behaviors->items[index]->destroying) {
            gscript_behavior__behaviors_call(behaviors, behaviors->items[index]->asset, "update", entities[i].entity);
        }
    }

    galloc_free(alloc, entities);
    return true;
}

static void gscript_behavior__behaviors_component_destroy(void* value) {
    gscript_behavior_Behavior* behavior = value;
    luaL_unref(behavior->lua, LUA_REGISTRYINDEX, behavior->state_ref);
}

static int gscript_behavior__behaviors_create_state(lua_State* lua) {
    lua_newtable(lua);
    lua_pushinteger(lua, luaL_ref(lua, LUA_REGISTRYINDEX));
    return 1;
}

static size_t gscript_behavior__behaviors_find_behavior(const gscript_behavior_Behaviors* behaviors, gecs_ID entity) {
    for (size_t i = 0; i < behaviors->count; i++) {
        if (behaviors->items[i]->entity == entity) {
            return i;
        }
    }

    return SIZE_MAX;
}

static void gscript_behavior__behaviors_remove_behavior(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    size_t index
) {
    gscript_behavior__behaviors_Behavior* old = behaviors->items[index];
    behaviors->count -= 1;
    behaviors->items[index] = behaviors->items[behaviors->count];
    gecs_Component_remove((gecs_Entity) {.world = behaviors->world, .id = old->entity}, behaviors->component);
    galloc_free(alloc, old);
}

static bool gscript_behavior__behaviors_reserve_behavior(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors
) {
    if (behaviors->count < behaviors->capacity) {
        return true;
    }

    size_t capacity = behaviors->capacity != 0 ? behaviors->capacity * 2 : 8;
    bool capacity_fits =
        capacity >= behaviors->capacity && capacity <= SIZE_MAX / sizeof(gscript_behavior__behaviors_Behavior*);
    if (!capacity_fits) {
        return false;
    }

    gscript_behavior__behaviors_Behavior** items =
        galloc_realloc(alloc, behaviors->items, behaviors->capacity * sizeof(*items), capacity * sizeof(*items));
    if (items == NULL) {
        return false;
    }

    behaviors->items = items;
    behaviors->capacity = capacity;
    return true;
}

static char* gscript_behavior__behaviors_behavior_copy_string(galloc_Allocator alloc, const char* text) {
    size_t size = strlen(text) + 1;
    char* copy = galloc_alloc(alloc, size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }

    return copy;
}

static int gscript_behavior__behaviors_behavior_error_protected(lua_State* lua) {
    const char* message = lua_touserdata(lua, 1);
    lua_pushstring(lua, message);
    return lua_error(lua);
}

static bool gscript_behavior__behaviors_behavior_error(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    const char* message
) {
    lua_State* lua = gscript_lua_state(behaviors->script);
    (void)alloc;
    // Protect string allocation so an exhausted Lua allocator returns the
    // preallocated Lua memory error rather than invoking its panic handler.
    lua_pushcfunction(lua, gscript_behavior__behaviors_behavior_error_protected);
    lua_pushlightuserdata(lua, (void*)message);
    (void)lua_pcall(lua, 1, 0, 0);
    return false;
}

// Script callback errors belong to the shared asset, not to one entity.
static void gscript_behavior__behaviors_call(
    gscript_behavior_Behaviors* behaviors,
    gscript_Asset* asset,
    const char* callback,
    gecs_ID entity
) {
    if (asset->error_state) {
        return;
    }

    lua_State* lua = gscript_lua_state(behaviors->script);
    int top = lua_gettop(lua);
    if (gscript_lua_call_module(
            behaviors->script,
            asset->module,
            callback,
            (gecs_Entity) {.world = behaviors->world, .id = entity}
        )) {
        return;
    }
    const char* message = lua_gettop(lua) > top && lua_type(lua, -1) == LUA_TSTRING
        ? lua_tostring(lua, -1)
        : "unknown Lua callback error";
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "[LUA] %s: %s", asset->path, message);
    glog_log(GLOG_ERROR, "%s", buffer);
    lua_settop(lua, top);
    asset->error_state = true;
}

static void gscript_behavior__behaviors_destroy_behavior(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    gecs_ID entity
) {
    size_t index = gscript_behavior__behaviors_find_behavior(behaviors, entity);
    if (index == SIZE_MAX || behaviors->items[index]->destroying) {
        return;
    }

    uint64_t version = behaviors->items[index]->version;
    gscript_Asset* asset = behaviors->items[index]->asset;
    behaviors->items[index]->destroying = true;
    gscript_behavior__behaviors_call(behaviors, asset, "destroy", entity);
    index = gscript_behavior__behaviors_find_behavior(behaviors, entity);
    if (index != SIZE_MAX && behaviors->items[index]->version == version) {
        gscript_behavior__behaviors_remove_behavior(alloc, behaviors, index);
    }
}

static bool gscript_behavior__behaviors_on_destroy(gecs_Entity handle, void* user) {
    gecs_World* world = handle.world;
    gecs_ID entity = handle.id;
    galloc_Allocator alloc = glint_allocator(world);
    gscript_behavior_Behaviors* behaviors = user;
    if (world != behaviors->world) {
        return false;
    }

    gscript_behavior__behaviors_destroy_behavior(alloc, behaviors, entity);
    return true;
}

static void gscript_behavior__behaviors_on_native_destroy(gecs_Entity handle, void* user) {
    gecs_ID entity = handle.id;
    gscript_behavior_Behaviors* behaviors = user;
    size_t index = gscript_behavior__behaviors_find_behavior(behaviors, entity);
    // A Lua-initiated destroy or manager shutdown may already be running this
    // hook; its outer frame owns the behavior and will remove it.
    if (index != SIZE_MAX && behaviors->items[index]->destroying) {
        return;
    }

    gscript_behavior__behaviors_destroy_behavior(behaviors->alloc, behaviors, entity);

    gscript_bindings_forget_entity(behaviors->script, handle);
}

static bool gscript_behavior__behaviors_on_attach(gecs_Entity handle, const char* path, void* user) {
    gscript_behavior_Behaviors* behaviors = user;
    return gscript_behavior_attach(behaviors, handle, path);
}

static void gscript_behavior__behaviors_reload_error(gscript_behavior_Behaviors* behaviors, gscript_Asset* asset) {
    lua_State* lua = gscript_lua_state(behaviors->script);
    const char* message = lua_tostring(lua, -1);
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "[LUA] %s: %s", asset->path, message != NULL ? message : "reload failed");
    glog_log(GLOG_ERROR, "%s", buffer);
    lua_pop(lua, 1);
    asset->dirty = false;
}

static bool gscript_behavior__behaviors_asset_reload(
    galloc_Allocator alloc,
    void* instance,
    const char* path,
    void* user
) {
    gscript_behavior_Behaviors* behaviors = user;
    gscript_Asset* asset = instance;
    size_t length;
    if (behaviors->reload_failed) {
        asset->dirty = true;
        return false;
    }

    char* source = gvfs_read(alloc, behaviors->vfs, path, &length);
    if (source == NULL) {
        gscript_behavior__behaviors_behavior_error(alloc, behaviors, "could not read behavior script");
        if (asset->module != LUA_NOREF) {
            gscript_behavior__behaviors_reload_error(behaviors, asset);
            return false;
        }

        asset->dirty = true;
        behaviors->reload_failed = true;
        return false;
    }

    int module = gscript_lua_load_module(alloc, behaviors->script, source, length, path);
    galloc_free(alloc, source);
    if (module == LUA_NOREF) {
        if (asset->module != LUA_NOREF) {
            gscript_behavior__behaviors_reload_error(behaviors, asset);
            return false;
        }

        asset->dirty = true;
        behaviors->reload_failed = true;
        return false;
    }

    if (asset->module != LUA_NOREF) {
        gscript_lua_release_module(alloc, behaviors->script, asset->module);
    }

    asset->module = module;
    asset->dirty = false;
    asset->error_state = false;
    return true;
}

static bool gscript_behavior__behaviors_asset_load(
    galloc_Allocator alloc,
    void* instance,
    const char* path,
    void* user
) {
    gscript_behavior_Behaviors* behaviors = user;
    gscript_Asset* asset = instance;
    asset->module = LUA_NOREF;
    asset->path = gscript_behavior__behaviors_behavior_copy_string(alloc, path);
    asset->native_path = gvfs_real_path(alloc, behaviors->vfs, path);
    if (asset->path == NULL
        || (asset->native_path != NULL
            && !gasset_Cache_watch(alloc, behaviors->cache, "lua", path, asset->native_path))) {
        gscript_behavior__behaviors_behavior_error(alloc, behaviors, "could not allocate behavior path or watcher");
        behaviors->reload_failed = true;
        galloc_free(alloc, asset->path);
        galloc_free(alloc, asset->native_path);
        return false;
    }

    if (!gscript_behavior__behaviors_asset_reload(alloc, instance, path, user)) {
        galloc_free(alloc, asset->path);
        galloc_free(alloc, asset->native_path);
        return false;
    }

    return true;
}

static void gscript_behavior__behaviors_asset_unload(galloc_Allocator alloc, void* instance, void* user) {
    gscript_Asset* asset = instance;
    gscript_behavior_Behaviors* behaviors = user;
    gscript_lua_release_module(alloc, behaviors->script, asset->module);
    galloc_free(alloc, asset->path);
    galloc_free(alloc, asset->native_path);
}

static gscript_Asset* gscript_behavior__behaviors_asset_get(
    galloc_Allocator alloc,
    gscript_behavior_Behaviors* behaviors,
    const char* path
) {
    behaviors->reload_failed = false;
    gscript_Asset* asset = gasset_Cache_load(alloc, behaviors->cache, "lua", path);
    if (asset == NULL && !behaviors->reload_failed) {
        gscript_behavior__behaviors_behavior_error(alloc, behaviors, "could not allocate behavior asset");
    }

    behaviors->reload_failed = false;
    return asset;
}

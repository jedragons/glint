#include <glint/behaviors.h>
#include <glint/assets/file_watcher.h>

#include <lauxlib.h>
#include <lua.h>
#include <stdint.h>
#include <string.h>

typedef struct gbeh__Behavior {
    gecs_Entity entity;
    int module;
    char *path;
    char *native_path;
    uint64_t version;
    bool dirty;
    bool destroying;
} gbeh__Behavior;

struct gbeh_Behaviors {
    galloc_Allocator alloc;
    gscript_Script *script;
    gecs_World *world;
    gvfs_Vfs *vfs;
    gwatch_Watcher *watcher;
    gbeh__Behavior *items;
    size_t count;
    size_t capacity;
    uint64_t version;
    bool shutting_down;
};

static size_t gbeh__behaviors_find_behavior(const gbeh_Behaviors *behaviors, gecs_Entity entity) {
    for (size_t i = 0; i < behaviors->count; i++) {
        if (behaviors->items[i].entity == entity)
            return i;
    }
    return SIZE_MAX;
}

static void gbeh__behaviors_remove_behavior(galloc_Allocator alloc, gbeh_Behaviors *behaviors, size_t index) {
    gbeh__Behavior old = behaviors->items[index];
    behaviors->items[index] = behaviors->items[--behaviors->count];
    gscript_release_module(alloc, behaviors->script, old.module);
    galloc_free(alloc, old.path);
    galloc_free(alloc, old.native_path);
}

static bool gbeh__behaviors_reserve_behavior(galloc_Allocator alloc, gbeh_Behaviors *behaviors) {
    gbeh__Behavior *items;
    size_t capacity;
    if (behaviors->count < behaviors->capacity)
        return true;
    capacity = behaviors->capacity ? behaviors->capacity * 2 : 8;
    if (capacity < behaviors->capacity || capacity > SIZE_MAX / sizeof(*items))
        return false;
    items = galloc_realloc(alloc, behaviors->items, behaviors->capacity * sizeof(*items), capacity * sizeof(*items));
    if (!items)
        return false;
    behaviors->items = items;
    behaviors->capacity = capacity;
    return true;
}

static char *gbeh__behaviors_behavior_copy_string(galloc_Allocator alloc, const char *text) {
    size_t size = strlen(text) + 1;
    char *copy = galloc_alloc(alloc, size);
    if (copy)
        memcpy(copy, text, size);
    return copy;
}

static int gbeh__behaviors_behavior_error_protected(lua_State *lua) {
    const char *message = lua_touserdata(lua, 1);
    lua_pushstring(lua, message);
    return lua_error(lua);
}

static bool gbeh__behaviors_behavior_error(galloc_Allocator alloc, gbeh_Behaviors *behaviors, const char *message) {
    lua_State *lua = gscript_state(behaviors->script);
    (void)alloc;
    /* Protect string allocation so an exhausted Lua allocator returns the
     * preallocated Lua memory error rather than invoking its panic handler. */
    lua_pushcfunction(lua, gbeh__behaviors_behavior_error_protected);
    lua_pushlightuserdata(lua, (void *)message);
    (void)lua_pcall(lua, 1, 0, 0);
    return false;
}

static bool gbeh__behaviors_destroy_behavior(galloc_Allocator alloc, gbeh_Behaviors *behaviors, gecs_Entity entity) {
    size_t index = gbeh__behaviors_find_behavior(behaviors, entity);
    uint64_t version;
    int module;
    bool ok;
    if (index == SIZE_MAX || behaviors->items[index].destroying)
        return true;
    version = behaviors->items[index].version;
    module = behaviors->items[index].module;
    behaviors->items[index].destroying = true;
    ok = gscript_call_module(alloc, behaviors->script, module, "destroy", behaviors->world, entity);
    index = gbeh__behaviors_find_behavior(behaviors, entity);
    if (index != SIZE_MAX && behaviors->items[index].version == version) {
        if (ok)
            gbeh__behaviors_remove_behavior(alloc, behaviors, index);
        else
            behaviors->items[index].destroying = false;
    }
    return ok;
}

static bool gbeh__behaviors_on_destroy(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, void *user) {
    gbeh_Behaviors *behaviors = user;
    return world == behaviors->world && gbeh__behaviors_destroy_behavior(alloc, behaviors, entity);
}

static void gbeh__behaviors_on_native_destroy(gecs_World *world, gecs_Entity entity, void *user) {
    gbeh_Behaviors *behaviors = user;
    size_t index = gbeh__behaviors_find_behavior(behaviors, entity);
    /* A Lua-initiated destroy or manager shutdown may already be running this
     * hook; its outer frame owns the behavior and will remove it. */
    if (index != SIZE_MAX && behaviors->items[index].destroying)
        return;
    if (!gbeh__behaviors_destroy_behavior(behaviors->alloc, behaviors, entity)) {
        lua_pop(gscript_state(behaviors->script), 1);
        index = gbeh__behaviors_find_behavior(behaviors, entity);
        if (index != SIZE_MAX)
            gbeh__behaviors_remove_behavior(behaviors->alloc, behaviors, index);
    }
    gscript_forget_entity(behaviors->alloc, behaviors->script, world, entity);
}

static bool gbeh__behaviors_on_attach(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, const char *path, void *user) {
    gbeh_Behaviors *behaviors = user;
    return world == behaviors->world && gbeh_attach(alloc, behaviors, entity, path);
}

gbeh_Behaviors *gbeh_new(galloc_Allocator alloc, gscript_Script *script, gecs_World *world, gvfs_Vfs *vfs) {
    gbeh_Behaviors *behaviors;
    if (!script || !world || !vfs)
        return NULL;
    behaviors = galloc_calloc(alloc, 1, sizeof(*behaviors));
    if (!behaviors)
        return NULL;
    behaviors->watcher = gwatch_create(alloc);
    if (!behaviors->watcher) {
        galloc_free(alloc, behaviors);
        return NULL;
    }
    if (!gecs_World_observe_destroy(world, gbeh__behaviors_on_native_destroy, behaviors)) {
        gwatch_destroy(alloc, behaviors->watcher);
        galloc_free(alloc, behaviors);
        return NULL;
    }
    behaviors->alloc = alloc;
    behaviors->script = script;
    behaviors->world = world;
    behaviors->vfs = vfs;
    gscript_set_entity_destroy(script, gbeh__behaviors_on_destroy, behaviors);
    gscript_set_entity_attach(script, gbeh__behaviors_on_attach, behaviors);
    return behaviors;
}

bool gbeh_attach(galloc_Allocator alloc, gbeh_Behaviors *behaviors, gecs_Entity entity, const char *path) {
    size_t length, index;
    char *source, *path_copy, *native_path;
    int module;
    uint64_t version;
    if (!behaviors)
        return false;
    if (!path || !gecs_Entity_alive(behaviors->world, entity)) {
        return gbeh__behaviors_behavior_error(alloc, behaviors, "invalid behavior path or entity");
    }
    if (behaviors->shutting_down)
        return gbeh__behaviors_behavior_error(alloc, behaviors, "cannot attach a behavior during shutdown");
    index = gbeh__behaviors_find_behavior(behaviors, entity);
    if (index != SIZE_MAX && behaviors->items[index].destroying) {
        return gbeh__behaviors_behavior_error(alloc, behaviors, "cannot attach a behavior during its destroy callback");
    }
    source = gvfs_read(alloc, behaviors->vfs, path, &length);
    if (!source)
        return gbeh__behaviors_behavior_error(alloc, behaviors, "could not read behavior script");
    module = gscript_load_module(alloc, behaviors->script, source, length, path);
    galloc_free(alloc, source);
    if (module == LUA_NOREF)
        return false;
    path_copy = gbeh__behaviors_behavior_copy_string(alloc, path);
    native_path = gvfs_real_path(alloc, behaviors->vfs, path);
    if (!path_copy || !native_path || !gwatch_add(alloc, behaviors->watcher, native_path)) {
        galloc_free(alloc, path_copy);
        galloc_free(alloc, native_path);
        gscript_release_module(alloc, behaviors->script, module);
        return gbeh__behaviors_behavior_error(alloc, behaviors, "could not allocate behavior path or watcher");
    }
    if (!gbeh__behaviors_destroy_behavior(alloc, behaviors, entity)) {
        galloc_free(alloc, path_copy);
        galloc_free(alloc, native_path);
        gscript_release_module(alloc, behaviors->script, module);
        return false;
    }
    if (!gecs_Entity_alive(behaviors->world, entity) || !gbeh__behaviors_reserve_behavior(alloc, behaviors)) {
        galloc_free(alloc, path_copy);
        galloc_free(alloc, native_path);
        gscript_release_module(alloc, behaviors->script, module);
        return gbeh__behaviors_behavior_error(alloc, behaviors, "entity was destroyed or behavior allocation failed");
    }
    version = ++behaviors->version;
    behaviors->items[behaviors->count++] = (gbeh__Behavior) {
        .entity = entity,
        .module = module,
        .path = path_copy,
        .native_path = native_path,
        .version = version
    };
    if (!gscript_call_module(alloc, behaviors->script, module, "setup", behaviors->world, entity)) {
        index = gbeh__behaviors_find_behavior(behaviors, entity);
        if (index != SIZE_MAX && behaviors->items[index].version == version)
            gbeh__behaviors_remove_behavior(alloc, behaviors, index);
        return false;
    }
    return true;
}

static void gbeh__behaviors_mark_dirty(const char *path, void *user) {
    gbeh_Behaviors *behaviors = user;
    for (size_t i = 0; i < behaviors->count; i++) {
        if (strcmp(behaviors->items[i].native_path, path) == 0)
            behaviors->items[i].dirty = true;
    }
}

static bool gbeh__behaviors_reload_behavior(galloc_Allocator alloc, gbeh_Behaviors *behaviors, gecs_Entity entity) {
    size_t index = gbeh__behaviors_find_behavior(behaviors, entity), length;
    uint64_t version;
    char *path, *source;
    int module;
    if (index == SIZE_MAX || !behaviors->items[index].dirty)
        return true;
    version = behaviors->items[index].version;
    path = gbeh__behaviors_behavior_copy_string(alloc, behaviors->items[index].path);
    if (!path)
        return gbeh__behaviors_behavior_error(alloc, behaviors, "could not allocate behavior path");
    source = gvfs_read(alloc, behaviors->vfs, path, &length);
    if (!source) {
        galloc_free(alloc, path);
        return gbeh__behaviors_behavior_error(alloc, behaviors, "could not read changed behavior script");
    }
    module = gscript_load_module(alloc, behaviors->script, source, length, path);
    galloc_free(alloc, source);
    galloc_free(alloc, path);
    if (module == LUA_NOREF)
        return false;
    index = gbeh__behaviors_find_behavior(behaviors, entity);
    if (index == SIZE_MAX || behaviors->items[index].version != version) {
        gscript_release_module(alloc, behaviors->script, module);
        return true;
    }
    gscript_release_module(alloc, behaviors->script, behaviors->items[index].module);
    behaviors->items[index].module = module;
    behaviors->items[index].dirty = false;
    return true;
}

bool gbeh_update(galloc_Allocator alloc, gbeh_Behaviors *behaviors) {
    typedef struct BehaviorSnapshot {
        gecs_Entity entity;
        uint64_t version;
    } BehaviorSnapshot;

    BehaviorSnapshot *entities;
    size_t count;
    bool ok = true;
    if (!behaviors)
        return false;
    gwatch_poll(behaviors->watcher, gbeh__behaviors_mark_dirty, behaviors);
    count = behaviors->count;
    if (!count)
        return true;
    if (count > SIZE_MAX / sizeof(*entities))
        return gbeh__behaviors_behavior_error(alloc, behaviors, "too many behaviors");
    entities = galloc_alloc(alloc, count * sizeof(*entities));
    if (!entities)
        return gbeh__behaviors_behavior_error(alloc, behaviors, "could not allocate behavior update list");
    for (size_t i = 0; i < count; i++)
        entities[i] = (BehaviorSnapshot) {.entity = behaviors->items[i].entity, .version = behaviors->items[i].version};
    for (size_t i = 0; i < count; i++) {
        size_t index = gbeh__behaviors_find_behavior(behaviors, entities[i].entity);
        if (index == SIZE_MAX || behaviors->items[index].version != entities[i].version)
            continue;
        if (!gecs_Entity_alive(behaviors->world, entities[i].entity)) {
            gscript_forget_entity(alloc, behaviors->script, behaviors->world, entities[i].entity);
            gbeh__behaviors_remove_behavior(alloc, behaviors, index);
            continue;
        }
        if (!gbeh__behaviors_reload_behavior(alloc, behaviors, entities[i].entity)) {
            ok = false;
            break;
        }
        index = gbeh__behaviors_find_behavior(behaviors, entities[i].entity);
        if (index != SIZE_MAX && behaviors->items[index].version == entities[i].version
            && !behaviors->items[index].destroying
            && !gscript_call_module(
                alloc,
                behaviors->script,
                behaviors->items[index].module,
                "update",
                behaviors->world,
                entities[i].entity
            )) {
            ok = false;
            break;
        }
    }
    galloc_free(alloc, entities);
    return ok;
}

void gbeh_destroy(galloc_Allocator alloc, gbeh_Behaviors *behaviors) {
    if (!behaviors)
        return;
    behaviors->shutting_down = true;
    while (behaviors->count) {
        gecs_Entity entity = behaviors->items[behaviors->count - 1].entity;
        if (gecs_Entity_alive(behaviors->world, entity)) {
            if (!gbeh__behaviors_destroy_behavior(alloc, behaviors, entity)) {
                lua_pop(gscript_state(behaviors->script), 1);
            }
        }
        size_t index = gbeh__behaviors_find_behavior(behaviors, entity);
        if (index != SIZE_MAX)
            gbeh__behaviors_remove_behavior(alloc, behaviors, index);
        gscript_forget_entity(alloc, behaviors->script, behaviors->world, entity);
    }
    gscript_set_entity_attach(behaviors->script, NULL, NULL);
    gscript_set_entity_destroy(behaviors->script, NULL, NULL);
    gecs_World_unobserve_destroy(behaviors->world, gbeh__behaviors_on_native_destroy, behaviors);
    gwatch_destroy(alloc, behaviors->watcher);
    galloc_free(alloc, behaviors->items);
    galloc_free(alloc, behaviors);
}

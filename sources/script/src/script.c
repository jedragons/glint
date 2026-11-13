#include <glint/script.h>
#include <glint/script_entity.h>

#include "script_internal.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

typedef struct gscript__DestroyFrame {
    gecs_World *world;
    gecs_Entity entity;
    struct gscript__DestroyFrame *previous;
} gscript__DestroyFrame;

struct gscript_Script {
    galloc_Allocator alloc;
    lua_State *lua;
    gscript_AttachFn attach_callback;
    void *attach_user;
    gscript_DestroyFn destroy_callback;
    void *destroy_user;
    gscript__DestroyFrame *destroying;
};

/* Lua's callback interface supplies an opaque context instead of an allocator
 * argument. The context holds the allocator explicitly supplied at creation. */
static void *gscript__script_lua_allocate(void *context, void *ptr, size_t old_size, size_t new_size) {
    galloc_Allocator alloc = *(galloc_Allocator *)context;
    if (new_size == 0) {
        galloc_free(alloc, ptr);
        return NULL;
    }
    if (ptr == NULL) {
        return galloc_alloc(alloc, new_size);
    }
    return galloc_realloc(alloc, ptr, old_size, new_size);
}

static int gscript__script_open_libraries(lua_State *lua) {
    void *context = NULL;
    (void)lua_getallocf(lua, &context);
    gscript_register_entity(*(galloc_Allocator *)context, lua);
    luaL_requiref(lua, LUA_GNAME, luaopen_base, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_DBLIBNAME, luaopen_debug, 1);
    lua_pop(lua, 1);
    return 0;
}

gscript_Script *gscript_new(galloc_Allocator alloc) {
    gscript_Script *script = galloc_calloc(alloc, 1, sizeof(*script));
    if (script == NULL) {
        return NULL;
    }
    script->alloc = alloc;
    script->lua = lua_newstate(gscript__script_lua_allocate, &script->alloc);
    if (script->lua == NULL) {
        galloc_free(alloc, script);
        return NULL;
    }
    lua_pushcfunction(script->lua, gscript__script_open_libraries);
    if (lua_pcall(script->lua, 0, 0, 0) != LUA_OK) {
        lua_close(script->lua);
        galloc_free(alloc, script);
        return NULL;
    }
    return script;
}

void gscript_destroy(galloc_Allocator alloc, gscript_Script *script) {
    if (script == NULL) {
        return;
    }
    lua_close(script->lua);
    galloc_free(alloc, script);
}

bool gscript_run(galloc_Allocator alloc, gscript_Script *script, const char *source, size_t length, const char *name) {
    (void)alloc;
    if (script == NULL || source == NULL) {
        return false;
    }
    if (luaL_loadbuffer(script->lua, source, length, name ? name : "script") != LUA_OK) {
        return false;
    }
    return lua_pcall(script->lua, 0, 0, 0) == LUA_OK;
}

lua_State *gscript_state(gscript_Script *script) {
    return script ? script->lua : NULL;
}

typedef struct gscript__PushEntityArgs {
    galloc_Allocator alloc;
    gecs_World *world;
    gecs_Entity entity;
} gscript__PushEntityArgs;

static int gscript__script_push_entity_protected(lua_State *lua) {
    gscript__PushEntityArgs *args = lua_touserdata(lua, 1);
    gscript_push_entity(args->alloc, lua, args->world, args->entity);
    return 1;
}

static int gscript__script_spawn_entity(lua_State *lua) {
    gecs_World *world = lua_touserdata(lua, lua_upvalueindex(1));
    void *context = NULL;
    (void)lua_getallocf(lua, &context);
    galloc_Allocator alloc = *(galloc_Allocator *)context;
    const char *path = lua_isnoneornil(lua, 1) ? NULL : luaL_checkstring(lua, 1);
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    if (!entity) {
        return luaL_error(lua, "could not create entity");
    }
    gscript__PushEntityArgs args = {.alloc = alloc, .world = world, .entity = entity};
    lua_pushcfunction(lua, gscript__script_push_entity_protected);
    lua_pushlightuserdata(lua, &args);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK) {
        gscript__entity_forget(lua, world, entity);
        gecs_Entity_destroy(world, entity);
        return lua_error(lua);
    }
    if (path && !gscript__dispatch_entity_attach(lua, gscript__entity_script(lua), world, entity, path)) {
        gscript__entity_forget(lua, world, entity);
        gecs_Entity_destroy(world, entity);
        return lua_error(lua);
    }
    return 1;
}

static int gscript__script_log_info(lua_State *lua) {
    int count = lua_gettop(lua);
    for (int i = 1; i <= count; i++) {
        const char *message = luaL_tolstring(lua, i, NULL);
        if (i > 1) {
            fputc(' ', stderr);
        }
        fputs(message, stderr);
        lua_pop(lua, 1);
    }
    fputc('\n', stderr);
    return 0;
}

static int gscript__script_bind_world_protected(lua_State *lua) {
    gecs_World *world = lua_touserdata(lua, 1);
    gscript_Script *script = lua_touserdata(lua, 2);
    gscript__entity_set_script(lua, script);
    void *context = NULL;
    (void)lua_getallocf(lua, &context);
    gscript__entity_bind_names(*(galloc_Allocator *)context, lua, world);
    lua_pushlightuserdata(lua, world);
    lua_pushcclosure(lua, gscript__script_spawn_entity, 1);
    lua_setglobal(lua, "spawn");
    lua_pushcfunction(lua, gscript__script_log_info);
    lua_setglobal(lua, "log_info");
    return 0;
}

bool gscript_bind_world(galloc_Allocator alloc, gscript_Script *script, gecs_World *world) {
    (void)alloc;
    if (!script || !world) {
        return false;
    }
    lua_pushcfunction(script->lua, gscript__script_bind_world_protected);
    lua_pushlightuserdata(script->lua, world);
    lua_pushlightuserdata(script->lua, script);
    return lua_pcall(script->lua, 2, 0, 0) == LUA_OK;
}

typedef struct gscript__LoadModuleArgs {
    const char *source;
    size_t length;
    const char *name;
} gscript__LoadModuleArgs;

static int gscript__script_load_module_protected(lua_State *lua) {
    gscript__LoadModuleArgs *args = lua_touserdata(lua, 1);
    if (luaL_loadbuffer(lua, args->source, args->length, args->name) != LUA_OK) {
        return lua_error(lua);
    }
    lua_call(lua, 0, 1);
    if (!lua_istable(lua, -1)) {
        return luaL_error(lua, "behavior script must return a table");
    }
    int reference = luaL_ref(lua, LUA_REGISTRYINDEX);
    lua_pushinteger(lua, reference);
    return 1;
}

int gscript_load_module(
    galloc_Allocator alloc,
    gscript_Script *script,
    const char *source,
    size_t length,
    const char *name
) {
    (void)alloc;
    if (script == NULL || source == NULL) {
        return LUA_NOREF;
    }
    lua_State *lua = script->lua;
    gscript__LoadModuleArgs args = {.source = source, .length = length, .name = name ? name : "script"};
    lua_pushcfunction(lua, gscript__script_load_module_protected);
    lua_pushlightuserdata(lua, &args);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK) {
        return LUA_NOREF;
    }
    int reference = (int)lua_tointeger(lua, -1);
    lua_pop(lua, 1);
    return reference;
}

typedef struct gscript__CallModuleArgs {
    galloc_Allocator alloc;
    int module;
    const char *callback;
    gecs_World *world;
    gecs_Entity entity;
} gscript__CallModuleArgs;

static int gscript__script_call_module_protected(lua_State *lua) {
    gscript__CallModuleArgs *args = lua_touserdata(lua, 1);
    lua_rawgeti(lua, LUA_REGISTRYINDEX, args->module);
    if (!lua_istable(lua, -1)) {
        return luaL_error(lua, "invalid behavior module");
    }
    lua_getfield(lua, -1, args->callback);
    if (lua_isnil(lua, -1)) {
        return 0;
    }
    if (!lua_isfunction(lua, -1)) {
        return luaL_error(lua, "behavior callback must be a function");
    }
    gscript_push_entity(args->alloc, lua, args->world, args->entity);
    lua_call(lua, 1, 0);
    return 0;
}

bool gscript_call_module(
    galloc_Allocator alloc,
    gscript_Script *script,
    int module,
    const char *callback,
    gecs_World *world,
    gecs_Entity entity
) {
    if (script == NULL || callback == NULL || world == NULL || !gecs_Entity_alive(world, entity)) {
        return false;
    }
    lua_State *lua = script->lua;
    gscript__CallModuleArgs args = {.alloc = alloc, .module = module, .callback = callback, .world = world, .entity = entity};
    lua_pushcfunction(lua, gscript__script_call_module_protected);
    lua_pushlightuserdata(lua, &args);
    return lua_pcall(lua, 1, 0, 0) == LUA_OK;
}

void gscript_release_module(galloc_Allocator alloc, gscript_Script *script, int module) {
    (void)alloc;
    if (script != NULL) {
        luaL_unref(script->lua, LUA_REGISTRYINDEX, module);
    }
}

void gscript_set_entity_destroy(gscript_Script *script, gscript_DestroyFn callback, void *user) {
    if (script != NULL) {
        script->destroy_callback = callback;
        script->destroy_user = user;
    }
}

void gscript_forget_entity(galloc_Allocator alloc, gscript_Script *script, gecs_World *world, gecs_Entity entity) {
    (void)alloc;
    if (script != NULL && world != NULL) {
        gscript__entity_forget(script->lua, world, entity);
    }
}

int gscript__dispatch_entity_destroy(lua_State *lua, gscript_Script *script, gecs_World *world, gecs_Entity entity) {
    gscript__DestroyFrame frame;
    int top = lua_gettop(lua);
    bool success;
    if (!script || !script->destroy_callback) {
        return 1;
    }
    for (gscript__DestroyFrame *active = script->destroying; active; active = active->previous) {
        if (active->world == world && active->entity == entity) {
            return 0;
        }
    }
    frame = (gscript__DestroyFrame) {.world = world, .entity = entity, .previous = script->destroying};
    script->destroying = &frame;
    success = script->destroy_callback(script->alloc, world, entity, script->destroy_user);
    script->destroying = frame.previous;
    if (!success) {
        if (lua_gettop(lua) <= top) {
            return luaL_error(lua, "entity destroy callback failed");
        }
        return lua_error(lua);
    }
    lua_settop(lua, top);
    return 1;
}

void gscript_set_entity_attach(gscript_Script *script, gscript_AttachFn callback, void *user) {
    if (script != NULL) {
        script->attach_callback = callback;
        script->attach_user = user;
    }
}

int gscript__dispatch_entity_attach(
    lua_State *lua,
    gscript_Script *script,
    gecs_World *world,
    gecs_Entity entity,
    const char *path
) {
    int top = lua_gettop(lua);
    if (!script || !script->attach_callback) {
        lua_pushliteral(lua, "script attachment is not configured");
        return 0;
    }
    if (!script->attach_callback(script->alloc, world, entity, path, script->attach_user)) {
        if (lua_gettop(lua) <= top) {
            lua_pushliteral(lua, "could not attach script");
        }
        return 0;
    }
    lua_settop(lua, top);
    return 1;
}

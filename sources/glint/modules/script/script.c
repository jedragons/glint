#include "./script.h"

#include "./bindings.h"
#include "./script_internal.h" // IWYU pragma: export

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>

typedef struct gscript__LoadModuleArgs {
    const char* source;
    size_t length;
    const char* name;
} gscript__LoadModuleArgs;

typedef struct gscript__CallModuleArgs {
    galloc_Allocator alloc;
    int module;
    const char* callback;
    gecs_World* world;
    gecs_ID entity;
} gscript__CallModuleArgs;

static void* gscript__lua_allocate(void* context, void* ptr, size_t old_size, size_t new_size);
static int gscript__open_libraries(lua_State* lua);
static int gscript__load_module_protected(lua_State* lua);
static int gscript__call_module_protected(lua_State* lua);

gscript_lua_Runtime* gscript_lua_new(galloc_Allocator alloc) {
    gscript_lua_Runtime* script = galloc_calloc(alloc, 1, sizeof(*script));
    if (script == NULL) {
        return NULL;
    }

    script->alloc = alloc;
    script->lua = lua_newstate(gscript__lua_allocate, &script->alloc);
    if (script->lua == NULL) {
        galloc_free(alloc, script);
        return NULL;
    }

    lua_pushcfunction(script->lua, gscript__open_libraries);
    if (lua_pcall(script->lua, 0, 0, 0) != LUA_OK) {
        lua_close(script->lua);
        galloc_free(alloc, script);
        return NULL;
    }

    return script;
}

void gscript_lua_destroy(galloc_Allocator alloc, gscript_lua_Runtime* script) {
    if (script == NULL) {
        return;
    }

    lua_close(script->lua);
    galloc_free(alloc, script);
}

bool gscript_lua_run(
    galloc_Allocator alloc,
    gscript_lua_Runtime* script,
    const char* source,
    size_t length,
    const char* name
) {
    (void)alloc;

    if (script == NULL || source == NULL) {
        return false;
    }

    if (luaL_loadbuffer(script->lua, source, length, name != NULL ? name : "script") != LUA_OK) {
        return false;
    }

    return lua_pcall(script->lua, 0, 0, 0) == LUA_OK;
}

lua_State* gscript_lua_state(gscript_lua_Runtime* script) {
    return script != NULL ? script->lua : NULL;
}

int gscript_lua_load_module(
    galloc_Allocator alloc,
    gscript_lua_Runtime* script,
    const char* source,
    size_t length,
    const char* name
) {
    (void)alloc;

    if (script == NULL || source == NULL) {
        return LUA_NOREF;
    }

    lua_State* lua = script->lua;
    gscript__LoadModuleArgs args = {.source = source, .length = length, .name = name != NULL ? name : "script"};
    lua_pushcfunction(lua, gscript__load_module_protected);
    lua_pushlightuserdata(lua, &args);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK) {
        return LUA_NOREF;
    }

    int reference = (int)lua_tointeger(lua, -1);
    lua_pop(lua, 1);
    return reference;
}

bool gscript_lua_call_module(gscript_lua_Runtime* script, int module, const char* callback, gecs_Entity handle) {
    gecs_World* world = handle.world;
    gecs_ID entity = handle.id;
    if (script == NULL || callback == NULL || world == NULL || !gecs_Entity_alive(handle)) {
        return false;
    }

    galloc_Allocator alloc = glint_allocator(world);

    lua_State* lua = script->lua;
    gscript__CallModuleArgs args = {
        .alloc = alloc,
        .module = module,
        .callback = callback,
        .world = world,
        .entity = entity,
    };

    lua_pushcfunction(lua, gscript__call_module_protected);
    lua_pushlightuserdata(lua, &args);
    return lua_pcall(lua, 1, 0, 0) == LUA_OK;
}

void gscript_lua_release_module(galloc_Allocator alloc, gscript_lua_Runtime* script, int module) {
    (void)alloc;
    if (script != NULL) {
        luaL_unref(script->lua, LUA_REGISTRYINDEX, module);
    }
}

void gscript_lua_set_entity_destroy(gscript_lua_Runtime* script, gscript_lua_DestroyFn callback, void* user) {
    if (script != NULL) {
        script->destroy_callback = callback;
        script->destroy_user = user;
    }
}

void gscript_lua_set_entity_attach(gscript_lua_Runtime* script, gscript_lua_AttachFn callback, void* user) {
    if (script != NULL) {
        script->attach_callback = callback;
        script->attach_user = user;
    }
}

void gscript_lua_take_error(gscript_lua_Runtime* script, char* error, size_t error_size) {
    lua_State* lua = gscript_lua_state(script);
    const char* message = lua_tostring(lua, -1);
    snprintf(error, error_size, "%s", message != NULL ? message : "Lua error");
    lua_pop(lua, 1);
}

static void* gscript__lua_allocate(void* context, void* ptr, size_t old_size, size_t new_size) {
    galloc_Allocator alloc = *(galloc_Allocator*)context;
    if (new_size == 0) {
        galloc_free(alloc, ptr);
        return NULL;
    }

    if (ptr == NULL) {
        return galloc_alloc(alloc, new_size);
    }

    return galloc_realloc(alloc, ptr, old_size, new_size);
}

static int gscript__open_libraries(lua_State* lua) {
    void* context = NULL;
    (void)lua_getallocf(lua, &context);
    gscript_bindings_register_entity(*(galloc_Allocator*)context, lua);
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
    gscript_bindings_register_logging(lua);
    return 0;
}

static int gscript__load_module_protected(lua_State* lua) {
    gscript__LoadModuleArgs* args = lua_touserdata(lua, 1);
    if (luaL_loadbuffer(lua, args->source, args->length, args->name) != LUA_OK) {
        return lua_error(lua);
    }

    lua_call(lua, 0, 1);
    if (lua_istable(lua, -1) == 0) {
        return luaL_error(lua, "behavior script must return a table");
    }

    int reference = luaL_ref(lua, LUA_REGISTRYINDEX);
    lua_pushinteger(lua, reference);
    return 1;
}

static int gscript__call_module_protected(lua_State* lua) {
    gscript__CallModuleArgs* args = lua_touserdata(lua, 1);
    lua_rawgeti(lua, LUA_REGISTRYINDEX, args->module);
    if (lua_istable(lua, -1) == 0) {
        return luaL_error(lua, "invalid behavior module");
    }

    lua_getfield(lua, -1, args->callback);
    if (lua_isnil(lua, -1) != 0) {
        return 0;
    }

    if (lua_isfunction(lua, -1) == 0) {
        return luaL_error(lua, "behavior callback must be a function");
    }

    gscript_bindings_push_entity(lua, (gecs_Entity) {.world = args->world, .id = args->entity});
    lua_call(lua, 1, 0);
    return 0;
}

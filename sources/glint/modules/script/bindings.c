#include "./bindings.h"
#include "./script_internal.h"

#include <glint/modules/names/names.h>
#include <glint/modules/script/behaviors.h>
#include <glint/platform/logging.h>

#include <lauxlib.h>

#include <string.h>

typedef struct gscript_bindings__DestroyFrame {
    gecs_World* world;
    gecs_ID entity;
    struct gscript_bindings__DestroyFrame* previous;
} gscript_bindings__DestroyFrame;

typedef struct gscript_bindings__PushEntityArgs {
    galloc_Allocator alloc;
    gecs_World* world;
    gecs_ID entity;
} gscript_bindings__PushEntityArgs;

static char gscript_bindings__registry_key;

static void gscript_bindings__forget(lua_State* lua, gecs_Entity entity);

typedef struct gscript_bindings__ScriptEntity {
    galloc_Allocator alloc;
    gecs_Entity handle;
} gscript_bindings__ScriptEntity;

static gscript_bindings__ScriptEntity* gscript_bindings__check_entity(lua_State* lua, int index);
static int gscript_bindings__entity_valid(lua_State* lua);
static int gscript_bindings__entity_attach_module(lua_State* lua);
static int gscript_bindings__entity_destroy(lua_State* lua);
static void gscript_bindings__push_data(lua_State* lua, gscript_bindings__ScriptEntity* value);
static int gscript_bindings__entity_index(lua_State* lua);
static int gscript_bindings__entity_newindex(lua_State* lua);
static int gscript_bindings__entity_find(lua_State* lua);

static int gscript_bindings__push_entity_protected(lua_State* lua);
static int gscript_bindings__spawn_entity(lua_State* lua);
static int gscript_bindings__log_message(lua_State* lua);
static void gscript_bindings__set_logger(lua_State* lua, const char* name, glog_Level level, bool fatal);
static int gscript_bindings__bind_world_protected(lua_State* lua);
static void gscript_bindings__entity_bind_names(lua_State* lua, gecs_World* world);
static void gscript_bindings__entity_set_script(lua_State* lua, gscript_lua_Runtime* script);
static gscript_lua_Runtime* gscript_bindings__entity_script(lua_State* lua);
static bool gscript_bindings__dispatch_entity_attach(
    lua_State* lua,
    gscript_lua_Runtime* script,
    gecs_Entity entity,
    const char* path
);
static bool gscript_bindings__dispatch_entity_destroy(lua_State* lua, gscript_lua_Runtime* script, gecs_Entity entity);

void gscript_bindings_register_entity(galloc_Allocator alloc, lua_State* lua) {
    (void)alloc;
    if (luaL_newmetatable(lua, "glint.Entity") != 0) {
        lua_pushcfunction(lua, gscript_bindings__entity_index);
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, gscript_bindings__entity_newindex);
        lua_setfield(lua, -2, "__newindex");
        lua_pushcfunction(lua, gscript_bindings__entity_valid);
        lua_setfield(lua, -2, "valid");
        lua_pushcfunction(lua, gscript_bindings__entity_destroy);
        lua_setfield(lua, -2, "destroy");
        lua_pushcfunction(lua, gscript_bindings__entity_attach_module);
        lua_setfield(lua, -2, "attach_module");
    }

    lua_pop(lua, 1);
}

void gscript_bindings_register_logging(lua_State* lua) {
    gscript_bindings__set_logger(lua, "log_trace", GLOG_TRACE, false);
    gscript_bindings__set_logger(lua, "log_debug", GLOG_DEBUG, false);
    gscript_bindings__set_logger(lua, "log_info", GLOG_INFO, false);
    gscript_bindings__set_logger(lua, "log_warn", GLOG_WARN, false);
    gscript_bindings__set_logger(lua, "log_error", GLOG_ERROR, false);
    gscript_bindings__set_logger(lua, "log_fatal", GLOG_ERROR, true);
}

void gscript_bindings_push_entity(lua_State* lua, gecs_Entity handle) {
    gecs_World* world = handle.world;
    galloc_Allocator alloc = glint_allocator(world);
    gscript_bindings__ScriptEntity* value = lua_newuserdatauv(lua, sizeof(*value), 0);
    *value = (gscript_bindings__ScriptEntity) {.alloc = alloc, .handle = handle};
    luaL_getmetatable(lua, "glint.Entity");
    lua_setmetatable(lua, -2);
}

gecs_Entity gscript_bindings_check_entity(lua_State* lua, int index) {
    gscript_bindings__ScriptEntity* value = gscript_bindings__check_entity(lua, index);
    return value->handle;
}

bool gscript_bindings_bind_world(gscript_lua_Runtime* script, gecs_World* world) {
    if (script == NULL || world == NULL) {
        return false;
    }

    lua_pushcfunction(script->lua, gscript_bindings__bind_world_protected);
    lua_pushlightuserdata(script->lua, world);
    lua_pushlightuserdata(script->lua, script);
    return lua_pcall(script->lua, 2, 0, 0) == LUA_OK;
}

void gscript_bindings_forget_entity(gscript_lua_Runtime* script, gecs_Entity handle) {
    gecs_World* world = handle.world;
    if (script != NULL && world != NULL) {
        gscript_bindings__forget(script->lua, handle);
    }
}

static void gscript_bindings__forget(lua_State* lua, gecs_Entity handle) {
    gecs_World* world = handle.world;
    gecs_ID entity = handle.id;
    lua_rawgetp(lua, LUA_REGISTRYINDEX, world);
    if (lua_istable(lua, -1) != 0) {
        lua_pushnil(lua);
        lua_rawseti(lua, -2, (lua_Integer)entity);
    }

    lua_pop(lua, 1);
}

static bool gscript_bindings__dispatch_entity_destroy(lua_State* lua, gscript_lua_Runtime* script, gecs_Entity handle) {
    gecs_World* world = handle.world;
    gecs_ID entity = handle.id;
    int top = lua_gettop(lua);
    if (script == NULL || script->destroy_callback == NULL) {
        return true;
    }

    for (gscript_bindings__DestroyFrame* active = script->destroying; active != NULL; active = active->previous) {
        if (active->world == world && active->entity == entity) {
            return false;
        }
    }

    gscript_bindings__DestroyFrame frame = {.world = world, .entity = entity, .previous = script->destroying};
    script->destroying = &frame;
    bool success = script->destroy_callback(handle, script->destroy_user);
    script->destroying = frame.previous;
    if (!success) {
        if (lua_gettop(lua) <= top) {
            luaL_error(lua, "entity destroy callback failed");
            return false;
        }

        lua_error(lua);
        return false;
    }

    lua_settop(lua, top);
    return true;
}

static bool gscript_bindings__dispatch_entity_attach(
    lua_State* lua,
    gscript_lua_Runtime* script,
    gecs_Entity handle,
    const char* path
) {
    int top = lua_gettop(lua);
    if (script == NULL || script->attach_callback == NULL) {
        lua_pushliteral(lua, "script attachment is not configured");
        return false;
    }

    if (!script->attach_callback(handle, path, script->attach_user)) {
        if (lua_gettop(lua) <= top) {
            lua_pushliteral(lua, "could not attach script");
        }

        return false;
    }

    lua_settop(lua, top);
    return true;
}

static void gscript_bindings__entity_bind_names(lua_State* lua, gecs_World* world) {
    lua_newtable(lua);
    lua_pushlightuserdata(lua, world);
    lua_pushboolean(lua, true);
    lua_pushcclosure(lua, gscript_bindings__entity_find, 2);
    lua_setfield(lua, -2, "find");
    lua_pushlightuserdata(lua, world);
    lua_pushboolean(lua, false);
    lua_pushcclosure(lua, gscript_bindings__entity_find, 2);
    lua_setfield(lua, -2, "try_find");
    lua_setglobal(lua, "Entity");
}

static gscript_lua_Runtime* gscript_bindings__entity_script(lua_State* lua) {
    lua_rawgetp(lua, LUA_REGISTRYINDEX, &gscript_bindings__registry_key);
    gscript_lua_Runtime* script = lua_touserdata(lua, -1);
    lua_pop(lua, 1);
    return script;
}

static void gscript_bindings__entity_set_script(lua_State* lua, gscript_lua_Runtime* script) {
    lua_pushlightuserdata(lua, script);
    lua_rawsetp(lua, LUA_REGISTRYINDEX, &gscript_bindings__registry_key);
}

static gscript_bindings__ScriptEntity* gscript_bindings__check_entity(lua_State* lua, int index) {
    return luaL_checkudata(lua, index, "glint.Entity");
}

static int gscript_bindings__entity_valid(lua_State* lua) {
    gscript_bindings__ScriptEntity* value = gscript_bindings__check_entity(lua, 1);
    lua_pushboolean(lua, gecs_Entity_alive(value->handle));
    return 1;
}

static int gscript_bindings__entity_attach_module(lua_State* lua) {
    gscript_bindings__ScriptEntity* value = gscript_bindings__check_entity(lua, 1);
    const char* path = luaL_checkstring(lua, 2);
    if (!gecs_Entity_alive(value->handle)) {
        return luaL_error(lua, "cannot attach a script to an invalid entity");
    }

    if (!gscript_bindings__dispatch_entity_attach(lua, gscript_bindings__entity_script(lua), value->handle, path)) {
        return lua_error(lua);
    }

    return 0;
}

static int gscript_bindings__entity_destroy(lua_State* lua) {
    gscript_bindings__ScriptEntity* value = gscript_bindings__check_entity(lua, 1);
    if (!gecs_Entity_alive(value->handle)) {
        return luaL_error(lua, "cannot destroy an invalid entity");
    }

    lua_rawgetp(lua, LUA_REGISTRYINDEX, &gscript_bindings__registry_key);
    gscript_lua_Runtime* script = lua_touserdata(lua, -1);
    lua_pop(lua, 1);
    if (!gscript_bindings__dispatch_entity_destroy(lua, script, value->handle)) {
        return 0;
    }

    gscript_bindings__forget(lua, value->handle);
    gecs_Entity_destroy(value->handle);
    return 0;
}

// Resolve on each access so old handles follow behavior replacement.
static void gscript_bindings__push_data(lua_State* lua, gscript_bindings__ScriptEntity* value) {
    gecs_Component type = gecs_Component_find(value->handle.world, "gscript_behavior_Behavior");
    gscript_behavior_Behavior* behavior = type != 0 ? gecs_Component_get(value->handle, type) : NULL;
    if (behavior != NULL) {
        lua_rawgeti(lua, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
        lua_State* main_thread = lua_tothread(lua, -1);
        lua_pop(lua, 1);
        if (behavior->lua != main_thread) {
            luaL_error(lua, "behavior belongs to a different Lua runtime");
        }

        lua_rawgeti(lua, LUA_REGISTRYINDEX, behavior->state_ref);
        return;
    }

    lua_rawgetp(lua, LUA_REGISTRYINDEX, value->handle.world);
    if (lua_isnil(lua, -1) != 0) {
        lua_pop(lua, 1);
        lua_newtable(lua);
        lua_pushvalue(lua, -1);
        lua_rawsetp(lua, LUA_REGISTRYINDEX, value->handle.world);
    }

    lua_rawgeti(lua, -1, (lua_Integer)value->handle.id);
    if (lua_isnil(lua, -1) != 0) {
        lua_pop(lua, 1);
        lua_newtable(lua);
        lua_pushvalue(lua, -1);
        lua_rawseti(lua, -3, (lua_Integer)value->handle.id);
    }

    lua_remove(lua, -2);
}

static int gscript_bindings__entity_index(lua_State* lua) {
    gscript_bindings__check_entity(lua, 1);
    luaL_getmetatable(lua, "glint.Entity");
    lua_pushvalue(lua, 2);
    lua_rawget(lua, -2);
    if (lua_isnil(lua, -1) == 0) {
        return 1;
    }

    lua_pop(lua, 2);
    gscript_bindings__ScriptEntity* value = gscript_bindings__check_entity(lua, 1);
    if (!gecs_Entity_alive(value->handle)) {
        return luaL_error(lua, "cannot access fields of an invalid entity");
    }

    if (lua_type(lua, 2) == LUA_TSTRING && strcmp(lua_tostring(lua, 2), "name") == 0) {
        const char* name = gname_get(value->handle);
        if (name != NULL) {
            lua_pushstring(lua, name);
        } else {
            lua_pushnil(lua);
        }

        return 1;
    }

    gscript_bindings__push_data(lua, value);
    lua_pushvalue(lua, 2);
    lua_rawget(lua, -2);
    return 1;
}

static int gscript_bindings__entity_newindex(lua_State* lua) {
    gscript_bindings__ScriptEntity* value = gscript_bindings__check_entity(lua, 1);
    if (!gecs_Entity_alive(value->handle)) {
        return luaL_error(lua, "cannot modify fields of an invalid entity");
    }

    if (lua_type(lua, 2) == LUA_TSTRING && strcmp(lua_tostring(lua, 2), "name") == 0) {
        size_t length = 0;
        const char* name = NULL;
        if (lua_isnil(lua, 3) == 0) {
            name = luaL_checklstring(lua, 3, &length);
            if (strlen(name) != length) {
                return luaL_error(lua, "entity name contains a zero byte");
            }
        }

        if (!gname_set(value->handle, name)) {
            return luaL_error(lua, "could not set entity name (duplicate name or allocation failure)");
        }

        return 0;
    }

    gscript_bindings__push_data(lua, value);
    lua_pushvalue(lua, 2);
    lua_pushvalue(lua, 3);
    lua_rawset(lua, -3);
    return 0;
}

static int gscript_bindings__entity_find(lua_State* lua) {
    gecs_World* world = lua_touserdata(lua, lua_upvalueindex(1));
    bool required = lua_toboolean(lua, lua_upvalueindex(2)) != 0;
    const char* name = luaL_checkstring(lua, 1);
    gecs_ID entity = gname_find(world, name).id;
    if (entity == 0) {
        if (required) {
            return luaL_error(lua, "entity with name '%s' does not exist", name);
        }

        lua_pushnil(lua);
        return 1;
    }

    void* context = NULL;
    (void)lua_getallocf(lua, &context);
    gscript_bindings_push_entity(lua, (gecs_Entity) {.world = world, .id = entity});
    return 1;
}

static int gscript_bindings__push_entity_protected(lua_State* lua) {
    gscript_bindings__PushEntityArgs* args = lua_touserdata(lua, 1);
    gscript_bindings_push_entity(lua, (gecs_Entity) {.world = args->world, .id = args->entity});
    return 1;
}

static int gscript_bindings__spawn_entity(lua_State* lua) {
    gecs_World* world = lua_touserdata(lua, lua_upvalueindex(1));
    void* context = NULL;
    (void)lua_getallocf(lua, &context);
    galloc_Allocator alloc = *(galloc_Allocator*)context;
    const char* path = lua_isnoneornil(lua, 1) ? NULL : luaL_checkstring(lua, 1);
    gecs_Entity entity = gecs_spawn(world);
    if (entity.id == 0) {
        return luaL_error(lua, "could not create entity");
    }

    gscript_bindings__PushEntityArgs args = {.alloc = alloc, .world = world, .entity = entity.id};
    lua_pushcfunction(lua, gscript_bindings__push_entity_protected);
    lua_pushlightuserdata(lua, &args);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK) {
        gscript_bindings__forget(lua, entity);
        gecs_Entity_destroy(entity);
        return lua_error(lua);
    }

    if (path != NULL
        && !gscript_bindings__dispatch_entity_attach(lua, gscript_bindings__entity_script(lua), entity, path)) {
        gscript_bindings__forget(lua, entity);
        gecs_Entity_destroy(entity);
        return lua_error(lua);
    }

    return 1;
}

static int gscript_bindings__log_message(lua_State* lua) {
    glog_Level level = (glog_Level)lua_tointeger(lua, lua_upvalueindex(1));
    bool fatal = lua_toboolean(lua, lua_upvalueindex(2)) != 0;
    int count = lua_gettop(lua);
    luaL_Buffer buffer;
    luaL_buffinit(lua, &buffer);
    for (int i = 1; i <= count; i++) {
        if (i > 1) {
            luaL_addchar(&buffer, ' ');
        }

        luaL_tolstring(lua, i, NULL);
        luaL_addvalue(&buffer);
    }

    luaL_pushresult(&buffer);
    glog_log(level, "%s", lua_tostring(lua, -1));
    if (fatal) {
        return luaL_error(lua, "log_fatal called");
    }

    return 0;
}

static void gscript_bindings__set_logger(lua_State* lua, const char* name, glog_Level level, bool fatal) {
    lua_pushinteger(lua, level);
    lua_pushboolean(lua, fatal);
    lua_pushcclosure(lua, gscript_bindings__log_message, 2);
    lua_setglobal(lua, name);
}

static int gscript_bindings__bind_world_protected(lua_State* lua) {
    gecs_World* world = lua_touserdata(lua, 1);
    gscript_lua_Runtime* script = lua_touserdata(lua, 2);
    gscript_bindings__entity_set_script(lua, script);
    gscript_bindings__entity_bind_names(lua, world);
    lua_pushlightuserdata(lua, world);
    lua_pushcclosure(lua, gscript_bindings__spawn_entity, 1);
    lua_setglobal(lua, "spawn");
    return 0;
}

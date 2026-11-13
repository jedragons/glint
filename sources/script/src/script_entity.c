#include <glint/script_entity.h>
#include <glint/names.h>

#include <string.h>

#include "script_internal.h"

#include <lauxlib.h>

static char gscript__registry_key;

typedef struct gscript__ScriptEntity {
    galloc_Allocator alloc;
    gecs_World *world;
    gecs_Entity entity;
} gscript__ScriptEntity;

static gscript__ScriptEntity *gscript__script_entity_script_check_entity(lua_State *lua, int index) {
    return luaL_checkudata(lua, index, "glint.Entity");
}

static int gscript__script_entity_entity_valid(lua_State *lua) {
    gscript__ScriptEntity *value = gscript__script_entity_script_check_entity(lua, 1);
    lua_pushboolean(lua, gecs_Entity_alive(value->world, value->entity));
    return 1;
}

void gscript__entity_set_script(lua_State *lua, gscript_Script *script) {
    lua_pushlightuserdata(lua, script);
    lua_rawsetp(lua, LUA_REGISTRYINDEX, &gscript__registry_key);
}

void gscript__entity_forget(lua_State *lua, gecs_World *world, gecs_Entity entity) {
    lua_rawgetp(lua, LUA_REGISTRYINDEX, world);
    if (lua_istable(lua, -1)) {
        lua_pushnil(lua);
        lua_rawseti(lua, -2, (lua_Integer)entity);
    }
    lua_pop(lua, 1);
}

gscript_Script *gscript__entity_script(lua_State *lua) {
    gscript_Script *script;
    lua_rawgetp(lua, LUA_REGISTRYINDEX, &gscript__registry_key);
    script = lua_touserdata(lua, -1);
    lua_pop(lua, 1);
    return script;
}

static int gscript__script_entity_entity_attach_module(lua_State *lua) {
    gscript__ScriptEntity *value = gscript__script_entity_script_check_entity(lua, 1);
    const char *path = luaL_checkstring(lua, 2);
    if (!gecs_Entity_alive(value->world, value->entity)) {
        return luaL_error(lua, "cannot attach a script to an invalid entity");
    }
    if (!gscript__dispatch_entity_attach(lua, gscript__entity_script(lua), value->world, value->entity, path)) {
        return lua_error(lua);
    }
    return 0;
}

static int gscript__script_entity_entity_destroy(lua_State *lua) {
    gscript__ScriptEntity *value = gscript__script_entity_script_check_entity(lua, 1);
    gscript_Script *script;
    if (!gecs_Entity_alive(value->world, value->entity)) {
        return luaL_error(lua, "cannot destroy an invalid entity");
    }
    lua_rawgetp(lua, LUA_REGISTRYINDEX, &gscript__registry_key);
    script = lua_touserdata(lua, -1);
    lua_pop(lua, 1);
    if (!gscript__dispatch_entity_destroy(lua, script, value->world, value->entity)) {
        return 0;
    }
    gscript__entity_forget(lua, value->world, value->entity);
    gecs_Entity_destroy(value->world, value->entity);
    return 0;
}

static int gscript__script_entity_entity_index(lua_State *lua) {
    gscript__script_entity_script_check_entity(lua, 1);
    luaL_getmetatable(lua, "glint.Entity");
    lua_pushvalue(lua, 2);
    lua_rawget(lua, -2);
    if (!lua_isnil(lua, -1)) {
        return 1;
    }
    lua_pop(lua, 2);
    gscript__ScriptEntity *value = gscript__script_entity_script_check_entity(lua, 1);
    if (!gecs_Entity_alive(value->world, value->entity)) {
        return luaL_error(lua, "cannot access fields of an invalid entity");
    }
    if (lua_type(lua, 2) == LUA_TSTRING && strcmp(lua_tostring(lua, 2), "name") == 0) {
        const char *name = gname_get(value->world, value->entity);
        if (name) {
            lua_pushstring(lua, name);
        } else {
            lua_pushnil(lua);
        }
        return 1;
    }
    lua_getiuservalue(lua, 1, 1);
    lua_pushvalue(lua, 2);
    lua_rawget(lua, -2);
    return 1;
}

static int gscript__script_entity_entity_newindex(lua_State *lua) {
    gscript__ScriptEntity *value = gscript__script_entity_script_check_entity(lua, 1);
    if (!gecs_Entity_alive(value->world, value->entity)) {
        return luaL_error(lua, "cannot modify fields of an invalid entity");
    }
    if (lua_type(lua, 2) == LUA_TSTRING && strcmp(lua_tostring(lua, 2), "name") == 0) {
        size_t length = 0;
        const char *name = NULL;
        if (!lua_isnil(lua, 3)) {
            name = luaL_checklstring(lua, 3, &length);
            if (strlen(name) != length) {
                return luaL_error(lua, "entity name contains a zero byte");
            }
        }
        if (!gname_set(value->alloc, value->world, value->entity, name)) {
            return luaL_error(lua, "could not set entity name (duplicate name or allocation failure)");
        }
        return 0;
    }
    lua_getiuservalue(lua, 1, 1);
    lua_pushvalue(lua, 2);
    lua_pushvalue(lua, 3);
    lua_rawset(lua, -3);
    return 0;
}

static int gscript__script_entity_entity_find(lua_State *lua) {
    gecs_World *world = lua_touserdata(lua, lua_upvalueindex(1));
    bool required = lua_toboolean(lua, lua_upvalueindex(2));
    const char *name = luaL_checkstring(lua, 1);
    gecs_Entity entity = gname_find(world, name);
    if (!entity) {
        if (required) {
            return luaL_error(lua, "entity with name '%s' does not exist", name);
        }
        lua_pushnil(lua);
        return 1;
    }
    void *context = NULL;
    (void)lua_getallocf(lua, &context);
    gscript_push_entity(*(galloc_Allocator *)context, lua, world, entity);
    return 1;
}

void gscript__entity_bind_names(galloc_Allocator alloc, lua_State *lua, gecs_World *world) {
    (void)alloc;
    lua_newtable(lua);
    lua_pushlightuserdata(lua, world);
    lua_pushboolean(lua, true);
    lua_pushcclosure(lua, gscript__script_entity_entity_find, 2);
    lua_setfield(lua, -2, "find");
    lua_pushlightuserdata(lua, world);
    lua_pushboolean(lua, false);
    lua_pushcclosure(lua, gscript__script_entity_entity_find, 2);
    lua_setfield(lua, -2, "try_find");
    lua_setglobal(lua, "Entity");
}

void gscript_register_entity(galloc_Allocator alloc, lua_State *lua) {
    (void)alloc;
    if (luaL_newmetatable(lua, "glint.Entity")) {
        lua_pushcfunction(lua, gscript__script_entity_entity_index);
        lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, gscript__script_entity_entity_newindex);
        lua_setfield(lua, -2, "__newindex");
        lua_pushcfunction(lua, gscript__script_entity_entity_valid);
        lua_setfield(lua, -2, "valid");
        lua_pushcfunction(lua, gscript__script_entity_entity_destroy);
        lua_setfield(lua, -2, "destroy");
        lua_pushcfunction(lua, gscript__script_entity_entity_attach_module);
        lua_setfield(lua, -2, "attach_module");
    }
    lua_pop(lua, 1);
}

void gscript_push_entity(galloc_Allocator alloc, lua_State *lua, gecs_World *world, gecs_Entity entity) {
    gscript__ScriptEntity *value = lua_newuserdatauv(lua, sizeof(*value), 1);
    *value = (gscript__ScriptEntity) {.alloc = alloc, .world = world, .entity = entity};
    luaL_getmetatable(lua, "glint.Entity");
    lua_setmetatable(lua, -2);

    /* Keep entity data in the Lua registry so all handles share fields. */
    lua_rawgetp(lua, LUA_REGISTRYINDEX, world);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        lua_newtable(lua);
        lua_pushvalue(lua, -1);
        lua_rawsetp(lua, LUA_REGISTRYINDEX, world);
    }
    lua_rawgeti(lua, -1, (lua_Integer)entity);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        lua_newtable(lua);
        lua_pushvalue(lua, -1);
        lua_rawseti(lua, -3, (lua_Integer)entity);
    }
    lua_setiuservalue(lua, -3, 1);
    lua_pop(lua, 1);
}

void gscript_check_entity(lua_State *lua, int index, gecs_World **world, gecs_Entity *entity) {
    gscript__ScriptEntity *value = gscript__script_entity_script_check_entity(lua, index);
    if (world != NULL) {
        *world = value->world;
    }
    if (entity != NULL) {
        *entity = value->entity;
    }
}

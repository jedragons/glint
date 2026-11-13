#include <glint/window_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <raylib.h>

#include <string.h>

#define GWINDOW_META "glint.Window"

typedef struct gwindow__WindowBindings {
    galloc_Allocator alloc;
    gwindow_FrameTimeFn frame_time;
    void *user;
} gwindow__WindowBindings;

static gwindow__WindowBindings *gwindow__window_lua_window_bindings(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static int gwindow__window_lua_window_index(lua_State *lua) {
    gwindow__WindowBindings *bindings = gwindow__window_lua_window_bindings(lua);
    const char *key = luaL_checkstring(lua, 2);
    if (strcmp(key, "dt") == 0) {
        lua_pushnumber(lua, bindings->frame_time ? bindings->frame_time(bindings->user) : GetFrameTime());
    } else if (strcmp(key, "time") == 0) {
        lua_pushnumber(lua, GetTime());
    } else if (strcmp(key, "width") == 0) {
        lua_pushinteger(lua, GetScreenWidth());
    } else if (strcmp(key, "height") == 0) {
        lua_pushinteger(lua, GetScreenHeight());
    } else {
        lua_pushnil(lua);
    }
    return 1;
}

static int gwindow__window_lua_window_newindex(lua_State *lua) {
    (void)luaL_checkstring(lua, 2);
    return luaL_error(lua, "Window properties are read-only");
}

static int gwindow__window_lua_log_message(lua_State *lua) {
    gwindow__WindowBindings *bindings = gwindow__window_lua_window_bindings(lua);
    int level = (int)lua_tointeger(lua, lua_upvalueindex(2));
    bool fatal = lua_toboolean(lua, lua_upvalueindex(3));
    int count = lua_gettop(lua);
    luaL_Buffer buffer;
    /* The Lua C callback ABI supplies no allocator argument. The captured
     * allocator is the one used to create the Lua state and its buffer. */
    (void)bindings->alloc;
    luaL_buffinit(lua, &buffer);
    for (int i = 1; i <= count; i++) {
        size_t length = 0;
        const char *part = lua_tolstring(lua, i, &length);
        if (!part) {
            part = "<unknown>";
            length = sizeof("<unknown>") - 1;
        }
        if (i > 1) {
            luaL_addchar(&buffer, ' ');
        }
        luaL_addlstring(&buffer, part, length);
    }
    luaL_pushresult(&buffer);
    TraceLog(level, "%s", lua_tostring(lua, -1));
    if (fatal) {
        return luaL_error(lua, "log_fatal called");
    }
    return 0;
}

static void gwindow__window_lua_set_logger(galloc_Allocator alloc, lua_State *lua, int context, const char *name, int level, bool fatal) {
    (void)alloc;
    lua_pushvalue(lua, context);
    lua_pushinteger(lua, level);
    lua_pushboolean(lua, fatal);
    lua_pushcclosure(lua, gwindow__window_lua_log_message, 3);
    lua_setglobal(lua, name);
}

bool gwindow_bind_lua(galloc_Allocator alloc, lua_State *lua, gwindow_FrameTimeFn frame_time, void *user) {
    if (!lua) {
        return false;
    }
    gwindow__WindowBindings *bindings = lua_newuserdatauv(lua, sizeof(*bindings), 0);
    *bindings = (gwindow__WindowBindings) {.alloc = alloc, .frame_time = frame_time, .user = user};
    int context = lua_gettop(lua);
    luaL_newmetatable(lua, GWINDOW_META);
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, gwindow__window_lua_window_index, 1);
    lua_setfield(lua, -2, "__index");
    lua_pushcfunction(lua, gwindow__window_lua_window_newindex);
    lua_setfield(lua, -2, "__newindex");
    lua_pop(lua, 1);
    lua_newuserdatauv(lua, 1, 0);
    luaL_setmetatable(lua, GWINDOW_META);
    lua_setglobal(lua, "window");
    lua_newtable(lua);
    lua_setglobal(lua, "Window");
    gwindow__window_lua_set_logger(alloc, lua, context, "log_trace", LOG_TRACE, false);
    gwindow__window_lua_set_logger(alloc, lua, context, "log_debug", LOG_DEBUG, false);
    gwindow__window_lua_set_logger(alloc, lua, context, "log_info", LOG_INFO, false);
    gwindow__window_lua_set_logger(alloc, lua, context, "log_warn", LOG_WARNING, false);
    gwindow__window_lua_set_logger(alloc, lua, context, "log_error", LOG_ERROR, false);
    gwindow__window_lua_set_logger(alloc, lua, context, "log_fatal", LOG_ERROR, true);
    lua_pop(lua, 1);
    return true;
}

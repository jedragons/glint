#include "./bindings.h"
#include <glint/platform/windowing.h>

#include <lauxlib.h>
#include <lua.h>

#include <string.h>

#define GWINDOW_BINDINGS_META "glint.Window"

typedef struct gwindow_bindings__WindowBindings {
    galloc_Allocator alloc;
    gwindow_FrameTimeFn frame_time;
    void* user;
} gwindow_bindings__WindowBindings;

static int gwindow_bindings__install(lua_State* lua);
static gwindow_bindings__WindowBindings* gwindow_bindings__window_bindings(lua_State* lua);
static int gwindow_bindings__window_index(lua_State* lua);
static int gwindow_bindings__window_newindex(lua_State* lua);

bool gwindow_bindings_bind(galloc_Allocator alloc, lua_State* lua, gwindow_FrameTimeFn frame_time, void* user) {
    if (lua == NULL) {
        return false;
    }

    gwindow_bindings__WindowBindings* bindings = lua_newuserdatauv(lua, sizeof(*bindings), 0);
    *bindings = (gwindow_bindings__WindowBindings) {.alloc = alloc, .frame_time = frame_time, .user = user};
    int context = lua_gettop(lua);
    luaL_newmetatable(lua, GWINDOW_BINDINGS_META);
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, gwindow_bindings__window_index, 1);
    lua_setfield(lua, -2, "__index");
    lua_pushcfunction(lua, gwindow_bindings__window_newindex);
    lua_setfield(lua, -2, "__newindex");
    lua_pop(lua, 1);
    lua_newuserdatauv(lua, 1, 0);
    luaL_setmetatable(lua, GWINDOW_BINDINGS_META);
    lua_setglobal(lua, "window");

    lua_newtable(lua);
    lua_setglobal(lua, "Window");
    lua_pop(lua, 1);
    return true;
}

bool gwindow_bindings_install(galloc_Allocator alloc, lua_State* lua, gwindow_FrameTimeFn frame_time, void* user) {
    if (lua == NULL) {
        return false;
    }

    gwindow_bindings__WindowBindings args = {.alloc = alloc, .frame_time = frame_time, .user = user};
    lua_pushcfunction(lua, gwindow_bindings__install);
    lua_pushlightuserdata(lua, &args);
    return lua_pcall(lua, 1, 0, 0) == LUA_OK;
}

static int gwindow_bindings__install(lua_State* lua) {
    gwindow_bindings__WindowBindings* args = lua_touserdata(lua, 1);
    if (!gwindow_bindings_bind(args->alloc, lua, args->frame_time, args->user)) {
        return luaL_error(lua, "could not bind window");
    }

    return 0;
}

static gwindow_bindings__WindowBindings* gwindow_bindings__window_bindings(lua_State* lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static int gwindow_bindings__window_index(lua_State* lua) {
    gwindow_bindings__WindowBindings* bindings = gwindow_bindings__window_bindings(lua);
    const char* key = luaL_checkstring(lua, 2);
    if (strcmp(key, "dt") == 0) {
        lua_pushnumber(
            lua,
            bindings->frame_time != NULL ? bindings->frame_time(bindings->user) : gplatform_frame_time()
        );
    } else if (strcmp(key, "time") == 0) {
        lua_pushnumber(lua, gplatform_time());
    } else if (strcmp(key, "width") == 0) {
        lua_pushinteger(lua, gplatform_width());
    } else if (strcmp(key, "height") == 0) {
        lua_pushinteger(lua, gplatform_height());
    } else {
        lua_pushnil(lua);
    }

    return 1;
}

static int gwindow_bindings__window_newindex(lua_State* lua) {
    (void)luaL_checkstring(lua, 2);
    return luaL_error(lua, "Window properties are read-only");
}

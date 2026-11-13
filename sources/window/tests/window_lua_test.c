#include <glint/window_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <mewunit.h>
#include <raylib.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int gwindow__test_levels[8];
static char gwindow__test_messages[8][128];
static size_t gwindow__test_logged;

float GetFrameTime(void) { return 0.5f; }
double GetTime(void) { return 123.5; }
int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }

void TraceLog(int level, const char *format, ...) {
    mewrequire("log buffer has room", gwindow__test_logged < 8);
    gwindow__test_levels[gwindow__test_logged] = level;
    va_list args;
    va_start(args, format);
    vsnprintf(gwindow__test_messages[gwindow__test_logged], sizeof(gwindow__test_messages[gwindow__test_logged]), format, args);
    va_end(args);
    gwindow__test_logged++;
}

static void *gwindow__test_lua_allocate(void *data, void *ptr, size_t old_size, size_t new_size) {
    galloc_Allocator alloc = *(galloc_Allocator *)data;
    if (!new_size) {
        galloc_free(alloc, ptr);
        return NULL;
    }
    if (!ptr) {
        return galloc_alloc(alloc, new_size);
    }
    return galloc_realloc(alloc, ptr, old_size, new_size);
}

static float gwindow__test_frame_time(void *user) {
    return *(float *)user;
}

static void gwindow__test_run(lua_State *lua, const char *source) {
    if (luaL_dostring(lua, source) != LUA_OK) {
        const char *message = lua_tostring(lua, -1);
        fprintf(stderr, "Lua error: %s\n", message ? message : "unknown");
        mewassert("Lua script succeeds", false);
    }
}

TEST(window_properties_and_logs, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    lua_State *lua = lua_newstate(gwindow__test_lua_allocate, &alloc);
    mewassert("Lua state created", lua);
    luaL_openlibs(lua);
    float dt = 0.25f;
    mewassert("window Lua bindings registered", gwindow_bind_lua(alloc, lua, gwindow__test_frame_time, &dt));
    gwindow__test_run(lua, "assert(window.dt == .25 and window.time == 123.5 and window.width == 800 and window.height == 600);"
             "assert(window.unknown == nil and type(Window) == 'table');"
             "assert(not pcall(function() window.width = 1 end));"
             "log_trace('trace'); log_debug('debug'); log_info('100% safe', 42, true);"
             "log_warn('warning'); log_error('error');"
             "local ok, err = pcall(log_fatal, 'fatal'); assert(not ok and err:find('log_fatal called'))");
    mewassert("all log functions called", gwindow__test_logged == 6);
    mewassert("trace level", gwindow__test_levels[0] == LOG_TRACE && strcmp(gwindow__test_messages[0], "trace") == 0);
    mewassert("debug level", gwindow__test_levels[1] == LOG_DEBUG && strcmp(gwindow__test_messages[1], "debug") == 0);
    mewassert("info arguments are safely formatted", gwindow__test_levels[2] == LOG_INFO &&
              strcmp(gwindow__test_messages[2], "100% safe 42 <unknown>") == 0);
    mewassert("warning level", gwindow__test_levels[3] == LOG_WARNING && strcmp(gwindow__test_messages[3], "warning") == 0);
    mewassert("error level", gwindow__test_levels[4] == LOG_ERROR && strcmp(gwindow__test_messages[4], "error") == 0);
    mewassert("fatal is logged at error level", gwindow__test_levels[5] == LOG_ERROR && strcmp(gwindow__test_messages[5], "fatal") == 0);
    dt = 1;
    gwindow__test_run(lua, "assert(window.dt == 1)");
    mewassert("default frame time binding", gwindow_bind_lua(alloc, lua, NULL, NULL));
    gwindow__test_run(lua, "assert(window.dt == .5)");
    lua_close(lua);
})

mewtest_main();

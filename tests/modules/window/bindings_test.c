#include <glint/modules/window/bindings.h>
#include <glint/platform/windowing.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>

static void* gwindow__test_lua_allocate(void* data, void* ptr, size_t old_size, size_t new_size);
static float gwindow__test_frame_time(void* user);
static bool gwindow__test_run(lua_State* lua, const char* source);

float gplatform_frame_time(void) {
    return 0.5f;
}

double gplatform_time(void) {
    return 123.5;
}

int gplatform_width(void) {
    return 800;
}

int gplatform_height(void) {
    return 600;
}

typedef struct gwindow__TestContext {
    galloc_Allocator alloc;
    lua_State* lua;
    float delta;
} gwindow__TestContext;

static bool gwindow__test_create(gwindow__TestContext* context);
static void gwindow__test_free(void* user);

GUNIT_TEST(window_host_properties, {
    gwindow__TestContext context = {0};
    gunit_defer(gwindow__test_free, &context);
    gunit_assert("create test context", gwindow__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gwindow__test_run(
            context.lua,
            "assert(window.dt == .25 and window.time == 123.5 and window.width == 800 and window.height == 600)"
        )
    );
    gunit_return();
})

GUNIT_TEST(window_unknown_property, {
    gwindow__TestContext context = {0};
    gunit_defer(gwindow__test_free, &context);
    gunit_assert("create test context", gwindow__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gwindow__test_run(context.lua, "assert(window.unknown == nil and type(Window) == 'table')")
    );
    gunit_return();
})

GUNIT_TEST(window_properties_are_readonly, {
    gwindow__TestContext context = {0};
    gunit_defer(gwindow__test_free, &context);
    gunit_assert("create test context", gwindow__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gwindow__test_run(context.lua, "assert(not pcall(function() window.width = 1 end))")
    );
    gunit_return();
})

GUNIT_TEST(window_frame_time_reads_host_each_time, {
    gwindow__TestContext context = {0};
    gunit_defer(gwindow__test_free, &context);
    gunit_assert("create test context", gwindow__test_create(&context));
    context.delta = 1;
    gunit_assert("test helper succeeds", gwindow__test_run(context.lua, "assert(window.dt == 1)"));
    gunit_return();
})

GUNIT_TEST(window_default_frame_time, {
    gwindow__TestContext context = {0};
    gunit_defer(gwindow__test_free, &context);
    gunit_assert("create test context", gwindow__test_create(&context));
    gunit_assert("default frame time binding", gwindow_bindings_bind(context.alloc, context.lua, NULL, NULL));
    gunit_assert("test helper succeeds", gwindow__test_run(context.lua, "assert(window.dt == .5)"));
    gunit_return();
})

gunit_main();

static bool gwindow__test_create(gwindow__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->delta = .25f;
    context->lua = lua_newstate(gwindow__test_lua_allocate, &context->alloc);
    if (!gunit_check("Lua state created", context->lua != NULL)) {
        return false;
    }
    luaL_openlibs(context->lua);
    if (!gunit_check(
            "window Lua bindings registered",
            gwindow_bindings_bind(context->alloc, context->lua, gwindow__test_frame_time, &context->delta)
        )) {
        return false;
    }
    return true;
}

static void gwindow__test_free(void* user) {
    gwindow__TestContext* context = user;
    if (context->lua != NULL) {
        lua_close(context->lua);
    }
}

static void* gwindow__test_lua_allocate(void* data, void* ptr, size_t old_size, size_t new_size) {
    galloc_Allocator alloc = *(galloc_Allocator*)data;
    if (new_size == 0) {
        galloc_free(alloc, ptr);
        return NULL;
    }

    if (ptr == NULL) {
        return galloc_alloc(alloc, new_size);
    }

    return galloc_realloc(alloc, ptr, old_size, new_size);
}

static float gwindow__test_frame_time(void* user) {
    return *(float*)user;
}

static bool gwindow__test_run(lua_State* lua, const char* source) {
    if (luaL_dostring(lua, source) != LUA_OK) {
        const char* message = lua_tostring(lua, -1);
        fprintf(stderr, "Lua error: %s\n", message != NULL ? message : "unknown");
        if (!gunit_check("Lua script succeeds", false)) {
            return false;
        }
    }
    return true;
}

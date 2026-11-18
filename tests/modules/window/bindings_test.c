#include <glint/modules/window/bindings.h>
#include <glint/platform/windowing.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

typedef struct gwindow__TestContext {
    galloc_Allocator alloc;
    lua_State* lua;
    float delta;
} gwindow__TestContext;

static bool gwindow__test_create(gwindow__TestContext* context);
static void gwindow__test_free(void* user);

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

GUNIT_TEST(window_properties,
    gwindow__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gwindow__test_free, &context);
        gunit_assert("bind window properties", gwindow__test_create(&context));
    })

    GUNIT_SECTION("reads values from the host", {
        const char* source =
            "assert(window.dt == 0.25)\n"
            "assert(window.time == 123.5)\n"
            "assert(window.width == 800)\n"
            "assert(window.height == 600)\n";
        gunit_assert("read host window properties", gwindow__test_run(context.lua, source));
    })

    GUNIT_SECTION("returns nil for an unknown property", {
        const char* source =
            "assert(window.unknown == nil)\n"
            "assert(type(Window) == 'table')\n";
        gunit_assert("read an unknown window property", gwindow__test_run(context.lua, source));
    })

    GUNIT_SECTION("rejects assigning to a property", {
        const char* source =
            "local wrote_width = pcall(function() window.width = 1 end)\n"
            "assert(not wrote_width)\n";
        gunit_assert("width is read-only", gwindow__test_run(context.lua, source));
    })

    GUNIT_SECTION("reads the latest frame time from the host", {
        context.delta = 1.0f;
        gunit_assert("read the new frame time", gwindow__test_run(context.lua, "assert(window.dt == 1)"));
    })

    GUNIT_SECTION("uses the platform frame time without a host callback", {
        gunit_assert("bind platform frame time", gwindow_bindings_bind(context.alloc, context.lua, NULL, NULL));
        gunit_assert("read the platform frame time", gwindow__test_run(context.lua, "assert(window.dt == 0.5)"));
    })
)

gunit_main();

static bool gwindow__test_create(gwindow__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->delta = 0.25f;
    context->lua = lua_newstate(gwindow__test_lua_allocate, &context->alloc);
    if (!gunit_check("Lua state created", context->lua != NULL)) {
        return false;
    }
    luaL_openlibs(context->lua);
    return gunit_check(
        "window Lua bindings registered",
        gwindow_bindings_bind(context->alloc, context->lua, gwindow__test_frame_time, &context->delta)
    );
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
    if (luaL_dostring(lua, source) == LUA_OK) {
        return true;
    }

    const char* message = lua_tostring(lua, -1);
    return gunit_check(message != NULL ? message : "Lua chunk failed", false);
}

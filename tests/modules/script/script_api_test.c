#include <glint/modules/script/bindings.h>
#include <glint/modules/script/script.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>

#include <string.h>

typedef struct gscript_api__TestContext {
    galloc_Allocator alloc;
    gscript_lua_Runtime* script;
    gecs_World* world;
    lua_State* lua;
    int module;
    gecs_Entity entity;
} gscript_api__TestContext;

static bool gscript_api__test_create(gscript_api__TestContext* context, bool bind_world);
static void gscript_api__test_free(void* user);
static bool gscript_api__test_module(gscript_api__TestContext* context);

GUNIT_TEST(runtime_runs_lua_chunk,
    gscript_api__TestContext context = {0};
    gunit_defer(gscript_api__test_free, &context);
    gunit_assert("create test context", gscript_api__test_create(&context, false));
    const char* source = "answer = math.floor(42.9)";
    gunit_assert("run Lua chunk", gscript_lua_run(context.alloc, context.script, source, strlen(source), "test"));
    lua_getglobal(context.lua, "answer");
    gunit_assert("chunk returns expected integer", lua_tointeger(context.lua, -1) == 42);
)

GUNIT_TEST(runtime_reports_invalid_lua,
    gscript_api__TestContext context = {0};
    gunit_defer(gscript_api__test_free, &context);
    gunit_assert("create test context", gscript_api__test_create(&context, false));
    gunit_assert("invalid Lua rejected", !gscript_lua_run(context.alloc, context.script, "invalid !", 9, "invalid"));
    gunit_assert("Lua error remains on stack", lua_isstring(context.lua, -1) != 0);
)

GUNIT_TEST(bound_entity_names_and_lifetime,
    gscript_api__TestContext context = {0};
    gunit_defer(gscript_api__test_free, &context);
    gunit_assert("create test context", gscript_api__test_create(&context, true));
    const char* source =
        "local e = spawn(); assert(e:valid()); e.name = 'hero'; "
        "assert(Entity.find('hero').name == 'hero'); assert(Entity.try_find('missing') == nil); "
        "e:destroy(); assert(not e:valid()); assert(Entity.try_find('hero') == nil)";
    gunit_assert(
        "entity name and lifetime API",
        gscript_lua_run(context.alloc, context.script, source, strlen(source), "spawn")
    );
)

GUNIT_TEST(module_callbacks_share_entity_state,
    gscript_api__TestContext context = {0};
    gunit_defer(gscript_api__test_free, &context);
    gunit_assert("create test context", gscript_api__test_create(&context, true));
    gunit_assert("test helper succeeds", gscript_api__test_module(&context));
    gunit_assert(
        "run setup callback",
        gscript_lua_call_module(context.script, context.module, "setup", context.entity)
    );
    gunit_assert(
        "run update callback",
        gscript_lua_call_module(context.script, context.module, "update", context.entity)
    );
    lua_getglobal(context.lua, "observed");
    gunit_assert("callback writes expected integer", lua_tointeger(context.lua, -1) == 42);
)

GUNIT_TEST(module_receives_live_entity_userdata,
    gscript_api__TestContext context = {0};
    gunit_defer(gscript_api__test_free, &context);
    gunit_assert("create test context", gscript_api__test_create(&context, true));
    gunit_assert("test helper succeeds", gscript_api__test_module(&context));
    gunit_assert(
        "run setup callback",
        gscript_lua_call_module(context.script, context.module, "setup", context.entity)
    );
    lua_getglobal(context.lua, "alive");
    gunit_assert("entity is alive in Lua", lua_toboolean(context.lua, -1) != 0);
    lua_getglobal(context.lua, "kind");
    gunit_assert("entity is Lua userdata", strcmp(lua_tostring(context.lua, -1), "userdata") == 0);
)

gunit_main();

static bool gscript_api__test_create(gscript_api__TestContext* context, bool bind_world) {
    context->alloc = galloc_malloc_allocator();
    context->module = LUA_NOREF;
    context->script = gscript_lua_new(context->alloc);
    if (!gunit_check("create runtime", context->script != NULL)) {
        return false;
    }
    context->lua = gscript_lua_state(context->script);
    if (bind_world) {
        context->world = gecs_World_new(context->alloc);
        if (!gunit_check("create world", context->world != NULL)) {
            return false;
        }
        if (!gunit_check("bind world", gscript_bindings_bind_world(context->script, context->world))) {
            return false;
        }
    }
    return true;
}

static void gscript_api__test_free(void* user) {
    gscript_api__TestContext* context = user;
    if (context->script != NULL) {
        if (context->module != LUA_NOREF) {
            gscript_lua_release_module(context->alloc, context->script, context->module);
        }
        gscript_lua_destroy(context->alloc, context->script);
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
}

static bool gscript_api__test_module(gscript_api__TestContext* context) {
    context->entity = gecs_spawn(context->world);
    if (!gunit_check("spawn callback target", context->entity.id != 0)) {
        return false;
    }
    const char* source =
        "return {setup = function(e) e.value = 42; kind = type(e); alive = e:valid() end, "
        "update = function(e) observed = e.value end}";
    context->module = gscript_lua_load_module(context->alloc, context->script, source, strlen(source), "behavior");
    if (!gunit_check("load behavior table", context->module != LUA_NOREF)) {
        return false;
    }
    return true;
}

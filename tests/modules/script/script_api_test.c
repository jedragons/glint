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

GUNIT_TEST(runtime_runs_lua_chunks,
    gscript_api__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_api__test_free, &context);
        gunit_assert("create Lua runtime", gscript_api__test_create(&context, false));
    })

    GUNIT_SECTION("runs a valid chunk", {
        const char* source = "answer = math.floor(42.9)";
        gunit_assert("run Lua chunk", gscript_lua_run(context.alloc, context.script, source, strlen(source), "test"));

        lua_getglobal(context.lua, "answer");
        gunit_assert("chunk sets the expected integer", lua_tointeger(context.lua, -1) == 42);
    })

    GUNIT_SECTION("reports a syntax error on the stack", {
        const char* source = "invalid !";
        gunit_assert(
            "invalid Lua rejected",
            !gscript_lua_run(context.alloc, context.script, source, strlen(source), "invalid")
        );
        gunit_assert("Lua error remains on stack", lua_isstring(context.lua, -1) != 0);
    })
)

GUNIT_TEST(bound_entity_names_and_lifetime,
    gscript_api__TestContext context = {0};
    gunit_defer(gscript_api__test_free, &context);
    gunit_assert("create runtime with a bound world", gscript_api__test_create(&context, true));
    const char* source =
        "local hero = spawn()\n"
        "assert(hero:valid())\n"
        "hero.name = 'hero'\n"
        "assert(Entity.find('hero').name == 'hero')\n"
        "assert(Entity.try_find('missing') == nil)\n"
        "hero:destroy()\n"
        "assert(not hero:valid())\n"
        "assert(Entity.try_find('hero') == nil)\n";
    gunit_assert(
        "entity name and lifetime API",
        gscript_lua_run(context.alloc, context.script, source, strlen(source), "spawn")
    );
)

GUNIT_TEST(module_entity_callbacks,
    gscript_api__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_api__test_free, &context);
        gunit_assert("create runtime with a bound world", gscript_api__test_create(&context, true));
        gunit_assert("load module with entity callbacks", gscript_api__test_module(&context));
        gunit_assert(
            "run setup callback",
            gscript_lua_call_module(context.script, context.module, "setup", context.entity)
        );
    })

    GUNIT_SECTION("passes a live entity as userdata to setup", {
        lua_getglobal(context.lua, "alive");
        gunit_assert("entity is alive in Lua", lua_toboolean(context.lua, -1) != 0);

        lua_getglobal(context.lua, "kind");
        const char* kind = lua_tostring(context.lua, -1);
        gunit_assert("entity type name is a string", kind != NULL);
        gunit_assert("entity is Lua userdata", strcmp(kind, "userdata") == 0);
    })

    GUNIT_SECTION("shares entity fields between callbacks", {
        gunit_assert(
            "run update callback",
            gscript_lua_call_module(context.script, context.module, "update", context.entity)
        );

        lua_getglobal(context.lua, "observed");
        gunit_assert("update sees the integer set by setup", lua_tointeger(context.lua, -1) == 42);
    })
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
        "return {\n"
        "    setup = function(entity)\n"
        "        entity.value = 42\n"
        "        kind = type(entity)\n"
        "        alive = entity:valid()\n"
        "    end,\n"
        "    update = function(entity) observed = entity.value end\n"
        "}\n";
    context->module = gscript_lua_load_module(context->alloc, context->script, source, strlen(source), "behavior");
    return gunit_check("load behavior table", context->module != LUA_NOREF);
}

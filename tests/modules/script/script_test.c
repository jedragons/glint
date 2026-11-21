#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/script.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>

#include <string.h>

typedef struct gscript__TestDestroy {
    gscript_lua_Runtime* script;
    int calls;
    bool fail;
    bool recurse;
} gscript__TestDestroy;

typedef struct gscript__TestContext {
    galloc_Allocator alloc;
    gecs_World* world;
    gscript_lua_Runtime* script;
    lua_State* lua;
    gecs_Entity entity;
    gscript__TestDestroy destroy;
} gscript__TestContext;

static bool gscript__test_create(gscript__TestContext* context);
static void gscript__test_free(void* user);
static bool gscript__test_on_destroy(gecs_Entity handle, void* user);
static bool gscript__test_run(gscript__TestContext* context, const char* source);

GUNIT_TEST(entity_handles_share_lua_fields,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create bound entity", gscript__test_create(&context));
    const char* set_fields =
        "e.number = 42\n"
        "e.data = {4, 5}\n"
        "assert(e:valid())\n";
    gunit_assert("set Lua fields on the first handle", gscript__test_run(&context, set_fields));

    gscript_bindings_push_entity(context.lua, context.entity);
    lua_setglobal(context.lua, "other");
    gunit_assert(
        "read the same Lua fields through the second handle",
        gscript__test_run(
            &context,
            "assert(other.number == 42)\n"
            "assert(other.data[2] == 5)\n"
        )
    );
)

GUNIT_TEST(entity_destroy_callback,
    gscript__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript__test_free, &context);
        gunit_assert("create entity with a destroy callback", gscript__test_create(&context));
    })

    GUNIT_SECTION("a veto leaves the entity alive", {
        context.destroy.fail = true;
        const char* source =
            "local destroyed, err = pcall(function() e:destroy() end)\n"
            "assert(not destroyed)\n"
            "assert(err == 'destroy rejected')\n"
            "assert(e:valid())\n";
        gunit_assert("report veto to Lua", gscript__test_run(&context, source));
        gunit_assert("destroy callback runs once", context.destroy.calls == 1);
        gunit_assert("veto leaves the native entity alive", gecs_Entity_alive(context.entity));
    })

    GUNIT_SECTION("recursive destruction invalidates every handle", {
        gscript_bindings_push_entity(context.lua, context.entity);
        lua_setglobal(context.lua, "other");
        context.destroy.recurse = true;

        const char* source =
            "e:destroy()\n"
            "assert(not e:valid())\n"
            "assert(not other:valid())\n"
            "assert(not pcall(function() return e.number end))\n"
            "assert(not pcall(function() e.number = 5 end))\n";
        gunit_assert("Lua handles reject access after destruction", gscript__test_run(&context, source));
        gunit_assert("destroy callback runs once", context.destroy.calls == 1);
        gunit_assert("native entity destroyed", !gecs_Entity_alive(context.entity));
    })
)

GUNIT_TEST(entity_slots_do_not_retain_destroyed_lua_fields,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create bound entity", gscript__test_create(&context));
    const char* destroy =
        "e.number = 42\n"
        "e:destroy()\n";
    gunit_assert("destroy the entity that occupied the slot", gscript__test_run(&context, destroy));

    context.entity = gecs_spawn(context.world);
    gunit_assert("spawn replacement entity", context.entity.id != 0);
    gscript_bindings_push_entity(context.lua, context.entity);
    lua_setglobal(context.lua, "next_entity");
    gunit_assert(
        "new entity has no fields from the previous occupant",
        gscript__test_run(&context, "assert(next_entity.number == nil)")
    );
)

GUNIT_TEST(native_destroy_forgets_lua_handle,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create bound entity", gscript__test_create(&context));
    gecs_Entity_destroy(context.entity);
    gscript_bindings_forget_entity(context.script, context.entity);

    const char* source =
        "assert(not e:valid())\n"
        "assert(not pcall(function() return e.number end))\n";
    gunit_assert("native destruction invalidates the Lua handle", gscript__test_run(&context, source));
)

gunit_main();

static bool gscript__test_create(gscript__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->world = gecs_World_new(context->alloc);
    if (!gunit_check("create world", context->world != NULL)) {
        return false;
    }
    context->script = gscript_lua_new(context->alloc);
    if (!gunit_check("create runtime", context->script != NULL)) {
        return false;
    }
    if (!gunit_check("bind world", gscript_bindings_bind_world(context->script, context->world))) {
        return false;
    }
    context->destroy.script = context->script;
    gscript_lua_set_entity_destroy(context->script, gscript__test_on_destroy, &context->destroy);
    context->lua = gscript_lua_state(context->script);
    context->entity = gecs_spawn(context->world);
    if (!gunit_check("spawn entity", context->entity.id != 0)) {
        return false;
    }
    gscript_bindings_push_entity(context->lua, context->entity);
    lua_setglobal(context->lua, "e");
    return true;
}

static void gscript__test_free(void* user) {
    gscript__TestContext* context = user;
    if (context->script != NULL) {
        gscript_lua_destroy(context->alloc, context->script);
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
}

static bool gscript__test_on_destroy(gecs_Entity handle, void* user) {
    gscript__TestDestroy* test = user;
    lua_State* lua = gscript_lua_state(test->script);
    test->calls += 1;
    if (test->fail) {
        lua_pushliteral(lua, "destroy rejected");
        return false;
    }

    if (test->recurse) {
        const char* source =
            "e:destroy()\n"
            "assert(e:valid())\n";
        galloc_Allocator alloc = glint_allocator(handle.world);
        return gscript_lua_run(alloc, test->script, source, strlen(source), "reentrant destroy");
    }

    return true;
}

static bool gscript__test_run(gscript__TestContext* context, const char* source) {
    if (gscript_lua_run(context->alloc, context->script, source, strlen(source), "test")) {
        return true;
    }

    const char* message = lua_tostring(context->lua, -1);
    return gunit_check(message != NULL ? message : "Lua chunk failed", false);
}

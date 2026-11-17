#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/script.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>

#include <stdio.h>
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
static bool gscript__test_run(galloc_Allocator alloc, gscript_lua_Runtime* script, const char* code);

GUNIT_TEST(entity_handles_share_lua_fields,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create test context", gscript__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(context.alloc, context.script, "e.number = 42; e.data = {4,5}; assert(e:valid())")
    );
    gscript_bindings_push_entity(context.lua, context.entity);
    lua_setglobal(context.lua, "other");
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(context.alloc, context.script, "assert(other.number == 42 and other.data[2] == 5)")
    );
)

GUNIT_TEST(entity_destroy_veto_preserves_entity,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create test context", gscript__test_create(&context));
    context.destroy.fail = true;
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(
            context.alloc,
            context.script,
            "local ok, err = pcall(function() e:destroy() end); assert(not ok and err == 'destroy rejected'); assert(e:valid())"
        )
    );
    gunit_assert("destroy veto leaves entity alive", context.destroy.calls == 1 && gecs_Entity_alive(context.entity));
)

GUNIT_TEST(entity_destroy_reentry_invalidates_all_handles,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create test context", gscript__test_create(&context));
    gscript_bindings_push_entity(context.lua, context.entity);
    lua_setglobal(context.lua, "other");
    context.destroy.recurse = true;
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(
            context.alloc,
            context.script,
            "e:destroy(); assert(not e:valid()); assert(not other:valid()); "
            "assert(not pcall(function() return e.number end)); assert(not pcall(function() e.number = 5 end))"
        )
    );
    gunit_assert(
        "allowed destruction removes entity",
        context.destroy.calls == 1 && !gecs_Entity_alive(context.entity)
    );
)

GUNIT_TEST(entity_slots_do_not_retain_destroyed_lua_fields,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create test context", gscript__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(context.alloc, context.script, "e.number = 42; e:destroy()")
    );
    context.entity = gecs_spawn(context.world);
    gunit_assert("spawn replacement entity", context.entity.id != 0);
    gscript_bindings_push_entity(context.lua, context.entity);
    lua_setglobal(context.lua, "next_entity");
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(context.alloc, context.script, "assert(next_entity.number == nil)")
    );
)

GUNIT_TEST(native_destroy_forgets_lua_handle,
    gscript__TestContext context = {0};
    gunit_defer(gscript__test_free, &context);
    gunit_assert("create test context", gscript__test_create(&context));
    gecs_Entity_destroy(context.entity);
    gscript_bindings_forget_entity(context.script, context.entity);
    gunit_assert(
        "test helper succeeds",
        gscript__test_run(
            context.alloc,
            context.script,
            "assert(not e:valid()); assert(not pcall(function() return e.number end))"
        )
    );
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
    gecs_World* world = handle.world;
    gecs_ID entity = handle.id;
    galloc_Allocator alloc = glint_allocator(world);
    gscript__TestDestroy* test = user;
    lua_State* lua = gscript_lua_state(test->script);
    (void)world;
    (void)entity;
    test->calls += 1;
    if (test->fail) {
        lua_pushliteral(lua, "destroy rejected");
        return false;
    }

    if (test->recurse) {
        const char* code = "e:destroy(); assert(e:valid())";
        return gscript_lua_run(alloc, test->script, code, strlen(code), "reentrant destroy");
    }

    return true;
}

static bool gscript__test_run(galloc_Allocator alloc, gscript_lua_Runtime* script, const char* code) {
    if (!gscript_lua_run(alloc, script, code, strlen(code), "test")) {
        fprintf(stderr, "%s\n", lua_tostring(gscript_lua_state(script), -1));
        if (!gunit_check("Lua assertion chunk succeeds", false)) {
            return false;
        }
    }
    return true;
}

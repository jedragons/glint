#include <glint/alloc.h>
#include <glint/ecs.h>
#include <glint/script.h>
#include <glint/script_entity.h>

#include <lauxlib.h>
#include <lua.h>
#include <mewunit.h>
#include <stdio.h>
#include <string.h>

typedef struct gscript__TestDestroy {
    gscript_Script *script;
    int calls;
    bool fail;
    bool recurse;
} gscript__TestDestroy;

static bool gscript__test_on_destroy(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, void *user) {
    gscript__TestDestroy *test = user;
    lua_State *lua = gscript_state(test->script);
    (void)world;
    (void)entity;
    ++test->calls;
    if (test->fail) {
        lua_pushliteral(lua, "destroy rejected");
        return false;
    }
    if (test->recurse) {
        const char *code = "e:destroy(); assert(e:valid())";
        return gscript_run(alloc, test->script, code, strlen(code), "reentrant destroy");
    }
    return true;
}

static void gscript__test_run(galloc_Allocator alloc, gscript_Script *script, const char *code) {
    if (!gscript_run(alloc, script, code, strlen(code), "test")) {
        fprintf(stderr, "%s\n", lua_tostring(gscript_state(script), -1));
        mewassert("Test expectation failed", 0);
    }
}

TEST(script_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    gscript__TestDestroy test = {.script = script};
    lua_State *lua;
    gecs_Entity entity;
    mewassert("Test expectation failed", world && script && gscript_bind_world(alloc, script, world));
    gscript_set_entity_destroy(script, gscript__test_on_destroy, &test);
    lua = gscript_state(script);
    entity = gecs_Entity_new(alloc, world);
    mewassert("Test expectation failed", entity);
    gscript_push_entity(alloc, lua, world, entity);
    lua_setglobal(lua, "e");
    gscript__test_run(alloc, script, "e.number = 42; e.data = {4,5}; assert(e:valid())");
    gscript_push_entity(alloc, lua, world, entity);
    lua_setglobal(lua, "other");
    gscript__test_run(alloc, script, "assert(other.number == 42 and other.data[2] == 5)");
    test.fail = true;
    gscript__test_run(
        alloc,
        script,
        "local ok, err = pcall(function() e:destroy() end); assert(not ok and err == 'destroy rejected'); assert(e:valid())"
    );
    mewassert("Test expectation failed", test.calls == 1 && gecs_Entity_alive(world, entity));
    test.fail = false;
    test.recurse = true;
    gscript__test_run(
        alloc,
        script,
        "e:destroy(); assert(not e:valid()); assert(not other:valid()); assert(not pcall(function() return e.number end)); assert(not pcall(function() e.number = 5 end))"
    );
    mewassert("Test expectation failed", test.calls == 2 && !gecs_Entity_alive(world, entity));
    entity = gecs_Entity_new(alloc, world);
    mewassert("Test expectation failed", entity);
    gscript_push_entity(alloc, lua, world, entity);
    lua_setglobal(lua, "next_entity");
    gscript__test_run(alloc, script, "assert(next_entity.number == nil)");
    gecs_Entity_destroy(world, entity);
    gscript_forget_entity(alloc, script, world, entity);
    gscript__test_run(alloc, script, "assert(not next_entity:valid()); assert(not pcall(function() return next_entity.number end))");
    gscript_destroy(alloc, script);
    gecs_World_free(alloc, world);
})

mewtest_main();

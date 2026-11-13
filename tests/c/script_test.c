#include <glint/script.h>

#include <mewunit.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

TEST(script_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gscript_Script *script = gscript_new(alloc);
    const char *source = "answer = math.floor(42.9)";
    mewassert("Test expectation failed", script != NULL);
    mewassert("Test expectation failed", gscript_run(alloc, script, source, strlen(source), "test"));
    lua_State *lua = gscript_state(script);
    lua_getglobal(lua, "answer");
    mewassert("Test expectation failed", lua_tointeger(lua, -1) == 42);
    lua_pop(lua, 1);
    mewassert("Test expectation failed", !gscript_run(alloc, script, "invalid !", 9, "invalid"));
    mewassert("Test expectation failed", lua_isstring(lua, -1));
    lua_pop(lua, 1);
    gecs_World *world = gecs_World_new(alloc);
    mewassert("Test expectation failed", world);
    mewassert("Test expectation failed", gscript_bind_world(alloc, script, world));
    const char *spawn =
        "spawned = spawn(); assert(spawned:valid()); spawned.name = \"hero\"; assert(Entity.find(\"hero\").name == \"hero\"); assert(Entity.try_find(\"missing\") == nil); spawned:destroy(); assert(not spawned:valid()); assert(Entity.try_find(\"hero\") == nil)";
    mewassert("Test expectation failed", gscript_run(alloc, script, spawn, strlen(spawn), "spawn"));
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    const char *behavior =
        "return {setup = function(e) e.value = 42; kind = type(e); alive = e:valid() end, "
        "update = function(e) observed = e.value end}";
    int module = gscript_load_module(alloc, script, behavior, strlen(behavior), "behavior");
    mewassert("Test expectation failed", module != LUA_NOREF);
    mewassert("Test expectation failed", gscript_call_module(alloc, script, module, "setup", world, entity));
    mewassert("Test expectation failed", gscript_call_module(alloc, script, module, "update", world, entity));
    lua_getglobal(lua, "observed");
    mewassert("Test expectation failed", lua_tointeger(lua, -1) == 42);
    lua_pop(lua, 1);
    lua_getglobal(lua, "alive");
    mewassert("Test expectation failed", lua_toboolean(lua, -1));
    lua_pop(lua, 1);
    lua_getglobal(lua, "kind");
    mewassert("Test expectation failed", strcmp(lua_tostring(lua, -1), "userdata") == 0);
    lua_pop(lua, 1);
    gscript_release_module(alloc, script, module);
    gscript_destroy(alloc, script);
    gecs_World_free(alloc, world);
})

mewtest_main();

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <glint/script.h>
#include <glint/script_entity.h>
#include <glint/time_lua.h>
#include <glint/types_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <mewunit.h>
#include <stdio.h>
#include <string.h>

static float gtime__test_frame_time(void *user) {
    return *(float *)user;
}

TEST(time_lua_test, {
    static const char gtime__test_code[] =
        "Timer.set(e, 2, TimerMode.repeating)\n"
        "assert(Timer.has(e))\n"
        "local t = Timer.get(e)\n"
        "assert(t:tick().elapsed == .5)\n"
        "assert(not t:tick().is_finished)\n"
        "assert(t:tick(1).is_finished and t.times_finished_this_tick == 1)\n"
        "assert(t:tick(.5).elapsed == .5)\n"
        "t:pause():tick(3); assert(t.is_paused and t.elapsed == .5)\n"
        "t:unpause(); t.elapsed = 1; t.duration = 4; t.mode = TimerMode.once\n"
        "assert(t.remaining == 3 and t.fraction == .25)\n"
        "local sw = Stopwatch.set(e)\n"
        "assert(sw:tick().elapsed == .5)\n"
        "sw:pause():tick(2); assert(sw:is_paused() and sw.elapsed == .5)\n"
        "sw:unpause():tick(.5); assert(sw.elapsed == 1)\n"
        "local copy = Stopwatch.new(); copy.elapsed = 7\n"
        "Stopwatch.set(e, copy); assert(Stopwatch.get(e).elapsed == 7)\n"
        "Stopwatch.remove(e); assert(not Stopwatch.has(e))\n"
        "assert(not pcall(function() return sw.elapsed end))\n"
        "Timer.remove(e); assert(not Timer.has(e))\n"
        "assert(not pcall(function() return t.elapsed end))\n";
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    lua_State *lua;
    gecs_Entity entity;
    float dt = 0.5f;
    mewassert("Test expectation failed", world && script);
    lua = gscript_state(script);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    gtypes_LuaService lua_service = {.lua = lua};
    gtime_Host host = {.frame_time = gtime__test_frame_time, .user = &dt};
    gecs_Component lua_type = GECS_CONTEXT(alloc, world, gtypes_LuaService);
    gecs_Component host_type = GECS_CONTEXT(alloc, world, gtime_Host);
    mewrequire("time host services", modules && gecs_context_set(alloc, world, lua_type, &lua_service) &&
               gecs_context_set(alloc, world, host_type, &host));
    gmod_Module time = gtime_module();
    mewrequire("import time module", gmod_Modules_import(alloc, modules, &time) &&
               gmod_Modules_import(alloc, modules, &time) && gmod_Modules_load(alloc, modules));
    luaL_newmetatable(lua, "glint.Entity");
    lua_pop(lua, 1);
    entity = gecs_Entity_new(alloc, world);
    mewassert("Test expectation failed", entity);
    gscript_push_entity(alloc, lua, world, entity);
    lua_setglobal(lua, "e");
    if (!gscript_run(alloc, script, gtime__test_code, strlen(gtime__test_code), "time test")) {
        fprintf(stderr, "%s\n", lua_tostring(lua, -1));
        mewassert("Test expectation failed", 0);
    }
    gmod_Modules_free(alloc, modules);
    gscript_destroy(alloc, script);
    gecs_World_free(alloc, world);
})

TEST(time_lua_rebind_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *first = gecs_World_new(alloc), *second = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    lua_State *lua = gscript_state(script);
    gecs_Entity e1 = gecs_Entity_new(alloc, first), e2 = gecs_Entity_new(alloc, second);
    gecs_Component small1 = gecs_Component_register(alloc, first, "small1", 1, NULL);
    gecs_Component small2 = gecs_Component_register(alloc, first, "small2", 1, NULL);
    char byte = 42;
    float dt1 = .5f, dt2 = .25f;
    const char *create =
        "old_timer = Timer; old_stopwatch = Stopwatch\n"
        "t = Timer.set(e1, 10, TimerMode.once); tg = Timer.get(e1)\n"
        "s = Stopwatch.set(e1); sg = Stopwatch.get(e1)\n"
        "standalone_t = Timer.new(10, TimerMode.once); standalone_s = Stopwatch.new()\n";
    const char *check =
        "assert(old_timer.get(e1).elapsed == 0 and old_stopwatch.get(e1).elapsed == 0)\n"
        "old_timer = nil; old_stopwatch = nil\n"
        "collectgarbage('collect')\n"
        "assert(t:tick().elapsed == .5 and s:tick().elapsed == .5)\n"
        "assert(tg.elapsed == .5 and sg.elapsed == .5)\n"
        "assert(standalone_t:tick().elapsed == .5 and standalone_s:tick().elapsed == .5)\n"
        "local nt = Timer.set(e2, t); local ns = Stopwatch.set(e2, s)\n"
        "assert(nt:tick().elapsed == .75 and ns:tick().elapsed == .75)\n"
        "assert(t.elapsed == .5 and s.elapsed == .5)\n"
        "t.elapsed = 2; s.elapsed = 3\n"
        "assert(tg.elapsed == 2 and sg.elapsed == 3)\n";
    mewassert("worlds, script and components", first && second && script && e1 && e2 && small1 && small2);
    mewassert("small components", gecs_Component_set(alloc, first, e1, small1, &byte) &&
                                   gecs_Component_set(alloc, first, e1, small2, &byte));
    luaL_newmetatable(lua, "glint.Entity");
    lua_pop(lua, 1);
    mewassert("bind first", gtime_bind_lua(alloc, lua, first, gtime__test_frame_time, &dt1));
    gscript_push_entity(alloc, lua, first, e1);
    lua_setglobal(lua, "e1");
    mewassert("create old handles", gscript_run(alloc, script, create, strlen(create), "create"));
    mewassert("bind second", gtime_bind_lua(alloc, lua, second, gtime__test_frame_time, &dt2));
    mewassert("different timer ids", gecs_Component_find(first, "gtime_Timer") != gecs_Component_find(second, "gtime_Timer"));
    mewassert("different stopwatch ids", gecs_Component_find(first, "gtime_Stopwatch") != gecs_Component_find(second, "gtime_Stopwatch"));
    gscript_push_entity(alloc, lua, second, e2);
    lua_setglobal(lua, "e2");
    if (!gscript_run(alloc, script, check, strlen(check), "rebind")) {
        fprintf(stderr, "%s\n", lua_tostring(lua, -1));
        mewassert("rebound handles", 0);
    }
    mewassert("small components unchanged", *(char *)gecs_Component_get(first, e1, small1) == 42 &&
                                              *(char *)gecs_Component_get(first, e1, small2) == 42);
    gscript_destroy(alloc, script);
    gecs_World_free(alloc, first);
    gecs_World_free(alloc, second);
})

mewtest_main();

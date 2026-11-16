#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/script.h>
#include <glint/modules/time/bindings.h>
#include <glint/modules/time/module.h>
#include <glint/modules/types/bindings.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>

#include <stdio.h>
#include <string.h>

static float gtime__test_frame_time(void* user);

typedef struct gtime__TestContext {
    galloc_Allocator alloc;
    gecs_World* first;
    gecs_World* second;
    gscript_lua_Runtime* script;
    lua_State* lua;
    gmod_Modules* modules;
    gecs_Entity e1;
    gecs_Entity e2;
    gecs_Component small1;
    gecs_Component small2;
    float delta1;
    float delta2;
} gtime__TestContext;

static bool gtime__test_create(gtime__TestContext* context, bool rebind);
static void gtime__test_free(void* user);
static bool gtime__test_run(gtime__TestContext* context, const char* source);

GUNIT_TEST(lua_repeating_timer_uses_host_delta, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "Timer.set(e, 2, TimerMode.repeating); assert(Timer.has(e)); local t=Timer.get(e); assert(t:tick().elapsed==.5); assert(not t:tick().is_finished); assert(t:tick(1).is_finished and t.times_finished_this_tick==1); assert(t:tick(.5).elapsed==.5)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_timer_pause_preserves_elapsed, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "local t=Timer.set(e, 2, TimerMode.repeating); t:tick(.5); t:pause():tick(3); assert(t.is_paused and t.elapsed==.5)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_timer_mutable_progress_properties, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "local t=Timer.set(e, 2, TimerMode.repeating); t:pause():unpause(); t.elapsed=1; t.duration=4; t.mode=TimerMode.once; assert(t.remaining==3 and t.fraction==.25)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_stopwatch_pause_and_resume, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "local sw=Stopwatch.set(e); assert(sw:tick().elapsed==.5); sw:pause():tick(2); assert(sw:is_paused() and sw.elapsed==.5); sw:unpause():tick(.5); assert(sw.elapsed==1)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_stopwatch_component_copies_value, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "local copy=Stopwatch.new(); copy.elapsed=7; Stopwatch.set(e, copy); assert(Stopwatch.get(e).elapsed==7)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_stopwatch_removed_reference_is_invalid, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "local sw=Stopwatch.set(e); Stopwatch.remove(e); assert(not Stopwatch.has(e)); assert(not pcall(function() return sw.elapsed end))"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_timer_removed_reference_is_invalid, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "local t=Timer.set(e, 2, TimerMode.once); Timer.remove(e); assert(not Timer.has(e)); assert(not pcall(function() return t.elapsed end))"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_rebind_old_types_retain_world, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, true));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(&context, "assert(old_timer.get(e1).elapsed==0 and old_stopwatch.get(e1).elapsed==0)")
    );
    gunit_return();
})

GUNIT_TEST(lua_rebind_retained_handles_keep_original_clock, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, true));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "old_timer=nil; old_stopwatch=nil; collectgarbage('collect'); assert(t:tick().elapsed==.5 and s:tick().elapsed==.5); assert(tg.elapsed==.5 and sg.elapsed==.5)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_rebind_standalone_values_keep_original_clock, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, true));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(&context, "assert(standalone_t:tick().elapsed==.5 and standalone_s:tick().elapsed==.5)")
    );
    gunit_return();
})

GUNIT_TEST(lua_rebind_copies_to_new_world_clock, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, true));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(
            &context,
            "t:tick(); s:tick(); local nt=Timer.set(e2,t); local ns=Stopwatch.set(e2,s); assert(nt:tick().elapsed==.75 and ns:tick().elapsed==.75); assert(t.elapsed==.5 and s.elapsed==.5)"
        )
    );
    gunit_return();
})

GUNIT_TEST(lua_rebind_old_handles_still_mutate_original_components, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, true));
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(&context, "t.elapsed=2; s.elapsed=3; assert(tg.elapsed==2 and sg.elapsed==3)")
    );
    gunit_return();
})

GUNIT_TEST(lua_rebind_does_not_alias_unrelated_components, {
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("create test context", gtime__test_create(&context, true));
    gunit_assert(
        "different timer IDs",
        gecs_Component_find(context.first, "gtime_Timer") != gecs_Component_find(context.second, "gtime_Timer")
    );
    gunit_assert(
        "different stopwatch IDs",
        gecs_Component_find(context.first, "gtime_Stopwatch") != gecs_Component_find(context.second, "gtime_Stopwatch")
    );
    gunit_assert(
        "test helper succeeds",
        gtime__test_run(&context, "t:tick(); s:tick(); Timer.set(e2,t):tick(); Stopwatch.set(e2,s):tick()")
    );
    gunit_assert(
        "small components unchanged",
        *(char*)gecs_Component_get(context.e1, context.small1) == 42
            && *(char*)gecs_Component_get(context.e1, context.small2) == 42
    );
    gunit_return();
})

gunit_main();

static bool gtime__test_create(gtime__TestContext* context, bool rebind) {
    context->alloc = galloc_malloc_allocator();
    context->delta1 = .5f;
    context->delta2 = .25f;
    context->first = gecs_World_new(context->alloc);
    if (!gunit_check("create first world", context->first != NULL)) {
        return false;
    }
    context->script = gscript_lua_new(context->alloc);
    if (!gunit_check("create script", context->script != NULL)) {
        return false;
    }
    context->lua = gscript_lua_state(context->script);
    luaL_newmetatable(context->lua, "glint.Entity");
    lua_pop(context->lua, 1);
    context->e1 = gecs_spawn(context->first);
    if (!gunit_check("spawn entity", context->e1.id != 0)) {
        return false;
    }
    if (rebind) {
        context->second = gecs_World_new(context->alloc);
        if (!gunit_check("create second world", context->second != NULL)) {
            return false;
        }
        context->e2 = gecs_spawn(context->second);
        context->small1 = gecs_Component_register(context->first, "small1", 1, NULL);
        context->small2 = gecs_Component_register(context->first, "small2", 1, NULL);
        if (!gunit_check(
                "register unrelated components",
                context->e2.id != 0 && context->small1 != 0 && context->small2 != 0
            )) {
            return false;
        }
        char byte = 42;
        if (!gunit_check(
                "set unrelated components",
                gecs_Component_set(context->e1, context->small1, &byte) != NULL
                    && gecs_Component_set(context->e1, context->small2, &byte) != NULL
            )) {
            return false;
        }
        if (!gunit_check(
                "bind first world",
                gtime_bindings_bind(context->lua, context->first, gtime__test_frame_time, &context->delta1)
            )) {
            return false;
        }
        gscript_bindings_push_entity(context->lua, context->e1);
        lua_setglobal(context->lua, "e1");
        if (!gtime__test_run(
                context,
                "old_timer=Timer; old_stopwatch=Stopwatch; t=Timer.set(e1,10,TimerMode.once); tg=Timer.get(e1); "
                "s=Stopwatch.set(e1); sg=Stopwatch.get(e1); standalone_t=Timer.new(10,TimerMode.once); standalone_s=Stopwatch.new()"
            )) {
            return false;
        }
        if (!gunit_check(
                "bind second world",
                gtime_bindings_bind(context->lua, context->second, gtime__test_frame_time, &context->delta2)
            )) {
            return false;
        }
        gscript_bindings_push_entity(context->lua, context->e2);
        lua_setglobal(context->lua, "e2");
    } else {
        context->modules = gmod_Modules_new(context->first);
        if (!gunit_check("create module manager", context->modules != NULL)) {
            return false;
        }
        gtypes_LuaService lua_service = {.lua = context->lua};
        gtime_Host host = {.frame_time = gtime__test_frame_time, .user = &context->delta1};
        if (!gunit_check(
                "provide Lua service",
                gecs_context_set(context->first, GECS_CONTEXT(context->first, gtypes_LuaService), &lua_service) != NULL
            )) {
            return false;
        }
        if (!gunit_check(
                "provide time host",
                gecs_context_set(context->first, GECS_CONTEXT(context->first, gtime_Host), &host) != NULL
            )) {
            return false;
        }
        gmod_Module time = gtime_module();
        if (!gunit_check(
                "import time idempotently",
                gmod_Modules_import(context->alloc, context->modules, time)
                    && gmod_Modules_import(context->alloc, context->modules, time)
            )) {
            return false;
        }
        if (!gunit_check("load time module", gmod_Modules_load(context->alloc, context->modules))) {
            return false;
        }
        gscript_bindings_push_entity(context->lua, context->e1);
        lua_setglobal(context->lua, "e");
    }
    return true;
}

static void gtime__test_free(void* user) {
    gtime__TestContext* context = user;
    if (context->modules != NULL) {
        gmod_Modules_free(context->alloc, context->modules);
    }
    if (context->script != NULL) {
        gscript_lua_destroy(context->alloc, context->script);
    }
    if (context->first != NULL) {
        gecs_World_free(context->first);
    }
    if (context->second != NULL) {
        gecs_World_free(context->second);
    }
}

static bool gtime__test_run(gtime__TestContext* context, const char* source) {
    if (!gscript_lua_run(context->alloc, context->script, source, strlen(source), "time binding")) {
        fprintf(stderr, "%s\n", lua_tostring(context->lua, -1));
        if (!gunit_check("Lua time behavior succeeds", false)) {
            return false;
        }
    }
    return true;
}

static float gtime__test_frame_time(void* user) {
    return *(float*)user;
}

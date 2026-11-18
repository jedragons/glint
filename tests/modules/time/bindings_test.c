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

GUNIT_TEST(lua_timer_components,
    gtime__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtime__test_free, &context);
        gunit_assert("bind time API", gtime__test_create(&context, false));
    })

    GUNIT_SECTION("ticks with host and explicit deltas", {
        const char* source =
            "Timer.set(e, 2, TimerMode.repeating)\n"
            "assert(Timer.has(e))\n"
            "local timer = Timer.get(e)\n"
            "assert(timer:tick().elapsed == 0.5)\n"
            "assert(not timer:tick().is_finished)\n"
            "assert(timer:tick(1).is_finished)\n"
            "assert(timer.times_finished_this_tick == 1)\n"
            "assert(timer:tick(0.5).elapsed == 0.5)\n";
        gunit_assert("ticks with host and explicit deltas", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("keeps elapsed time while paused", {
        const char* source =
            "local timer = Timer.set(e, 2, TimerMode.repeating)\n"
            "timer:tick(0.5)\n"
            "timer:pause():tick(3)\n"
            "assert(timer.is_paused)\n"
            "assert(timer.elapsed == 0.5)\n";
        gunit_assert("keeps elapsed time while paused", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("updates progress when its properties change", {
        const char* source =
            "local timer = Timer.set(e, 2, TimerMode.repeating)\n"
            "timer:pause():unpause()\n"
            "timer.elapsed = 1\n"
            "timer.duration = 4\n"
            "timer.mode = TimerMode.once\n"
            "assert(timer.remaining == 3)\n"
            "assert(timer.fraction == 0.25)\n";
        gunit_assert("updates progress when its properties change", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("invalidates references when removed", {
        const char* source =
            "local timer = Timer.set(e, 2, TimerMode.once)\n"
            "Timer.remove(e)\n"
            "assert(not Timer.has(e))\n"
            "assert(not pcall(function() return timer.elapsed end))\n";
        gunit_assert("invalidates references when removed", gtime__test_run(&context, source));
    })
)

GUNIT_TEST(lua_stopwatch_components,
    gtime__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtime__test_free, &context);
        gunit_assert("bind time API", gtime__test_create(&context, false));
    })

    GUNIT_SECTION("pauses and resumes an elapsed interval", {
        const char* source =
            "local stopwatch = Stopwatch.set(e)\n"
            "assert(stopwatch:tick().elapsed == 0.5)\n"
            "stopwatch:pause():tick(2)\n"
            "assert(stopwatch:is_paused())\n"
            "assert(stopwatch.elapsed == 0.5)\n"
            "stopwatch:unpause():tick(0.5)\n"
            "assert(stopwatch.elapsed == 1)\n";
        gunit_assert("pauses and resumes an elapsed interval", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("copies the supplied stopwatch value", {
        const char* source =
            "local value = Stopwatch.new()\n"
            "value.elapsed = 7\n"
            "Stopwatch.set(e, value)\n"
            "assert(Stopwatch.get(e).elapsed == 7)\n";
        gunit_assert("copies the supplied stopwatch value", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("invalidates references when removed", {
        const char* source =
            "local stopwatch = Stopwatch.set(e)\n"
            "Stopwatch.remove(e)\n"
            "assert(not Stopwatch.has(e))\n"
            "assert(not pcall(function() return stopwatch.elapsed end))\n";
        gunit_assert("invalidates references when removed", gtime__test_run(&context, source));
    })
)

GUNIT_TEST(lua_rebound_time_bindings,
    gtime__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtime__test_free, &context);
        gunit_assert("bind time API", gtime__test_create(&context, true));
    })

    GUNIT_SECTION("retained type tables resolve the original world", {
        const char* source =
            "assert(old_timer.get(e1).elapsed == 0)\n"
            "assert(old_stopwatch.get(e1).elapsed == 0)\n";
        gunit_assert("retained type tables resolve the original world", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("retained component handles use the original clock after collection", {
        const char* source =
            "old_timer = nil\n"
            "old_stopwatch = nil\n"
            "collectgarbage('collect')\n"
            "assert(first_timer:tick().elapsed == 0.5)\n"
            "assert(first_stopwatch:tick().elapsed == 0.5)\n"
            "assert(timer_alias.elapsed == 0.5)\n"
            "assert(stopwatch_alias.elapsed == 0.5)\n";
        gunit_assert("retained handles keep the original clock", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("standalone values use the original clock", {
        const char* source =
            "assert(standalone_timer:tick().elapsed == 0.5)\n"
            "assert(standalone_stopwatch:tick().elapsed == 0.5)\n";
        gunit_assert("standalone values use the original clock", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("copies use the destination world clock", {
        const char* source =
            "first_timer:tick()\n"
            "first_stopwatch:tick()\n"
            "local timer = Timer.set(e2, first_timer)\n"
            "local stopwatch = Stopwatch.set(e2, first_stopwatch)\n"
            "assert(timer:tick().elapsed == 0.75)\n"
            "assert(stopwatch:tick().elapsed == 0.75)\n"
            "assert(first_timer.elapsed == 0.5)\n"
            "assert(first_stopwatch.elapsed == 0.5)\n";
        gunit_assert("copies use the destination world clock", gtime__test_run(&context, source));
    })

    GUNIT_SECTION("retained handles still mutate their original components", {
        const char* source =
            "first_timer.elapsed = 2\n"
            "first_stopwatch.elapsed = 3\n"
            "assert(timer_alias.elapsed == 2)\n"
            "assert(stopwatch_alias.elapsed == 3)\n";
        gunit_assert("retained handles still mutate their original components", gtime__test_run(&context, source));
    })
)

GUNIT_TEST(lua_rebind_does_not_alias_unrelated_components,
    gtime__TestContext context = {0};
    gunit_defer(gtime__test_free, &context);
    gunit_assert("bind time API to both worlds", gtime__test_create(&context, true));

    gecs_Component first_timer = gecs_Component_find(context.first, "gtime_Timer");
    gecs_Component second_timer = gecs_Component_find(context.second, "gtime_Timer");
    gunit_assert("worlds use different timer IDs", first_timer != second_timer);

    gecs_Component first_stopwatch = gecs_Component_find(context.first, "gtime_Stopwatch");
    gecs_Component second_stopwatch = gecs_Component_find(context.second, "gtime_Stopwatch");
    gunit_assert("worlds use different stopwatch IDs", first_stopwatch != second_stopwatch);

    const char* source =
        "first_timer:tick()\n"
        "first_stopwatch:tick()\n"
        "Timer.set(e2, first_timer):tick()\n"
        "Stopwatch.set(e2, first_stopwatch):tick()\n";
    gunit_assert("tick the original and copied components", gtime__test_run(&context, source));

    const char* first_byte = gecs_Component_get(context.e1, context.small1);
    gunit_assert("first unrelated component is present", first_byte != NULL);
    gunit_assert("first unrelated component is unchanged", *first_byte == 42);

    const char* second_byte = gecs_Component_get(context.e1, context.small2);
    gunit_assert("second unrelated component is present", second_byte != NULL);
    gunit_assert("second unrelated component is unchanged", *second_byte == 42);
)

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
        const char* source =
            "old_timer = Timer\n"
            "old_stopwatch = Stopwatch\n"
            "first_timer = Timer.set(e1, 10, TimerMode.once)\n"
            "timer_alias = Timer.get(e1)\n"
            "first_stopwatch = Stopwatch.set(e1)\n"
            "stopwatch_alias = Stopwatch.get(e1)\n"
            "standalone_timer = Timer.new(10, TimerMode.once)\n"
            "standalone_stopwatch = Stopwatch.new()\n";
        if (!gtime__test_run(context, source)) {
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
    if (gscript_lua_run(context->alloc, context->script, source, strlen(source), "time binding")) {
        return true;
    }

    const char* message = lua_tostring(context->lua, -1);
    return gunit_check(message != NULL ? message : "Lua chunk failed", false);
}

static float gtime__test_frame_time(void* user) {
    return *(float*)user;
}

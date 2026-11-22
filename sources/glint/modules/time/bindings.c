#include "./bindings.h"
#include <glint/modules/script/bindings.h>
#include <glint/modules/time/time.h>

#include <lauxlib.h>
#include <lua.h>

#include <string.h>

#define GTIME_BINDINGS_TIMER_META "glint.Timer"
#define GTIME_BINDINGS_STOPWATCH_META "glint.Stopwatch"

typedef struct gtime_bindings__TimeBindings {
    galloc_Allocator alloc;
    gecs_World* world;
    gecs_Component timer;
    gecs_Component stopwatch;
    gtime_FrameTimeFn frame_time;
    void* frame_time_user;
} gtime_bindings__TimeBindings;

typedef struct gtime_bindings__TimeObject {
    gecs_ID entity;
    gecs_World* world;
    gtime_bindings__TimeBindings* bindings;

    union {
        gtime_Timer timer;
        gtime_Stopwatch stopwatch;
    } value;
} gtime_bindings__TimeObject;

typedef struct gtime_bindings__BindArgs {
    gecs_World* world;
    gtime_FrameTimeFn frame_time;
    void* user;
} gtime_bindings__BindArgs;

static int gtime_bindings__install(lua_State* lua);
static gtime_bindings__TimeBindings* gtime_bindings__bindings(lua_State* lua);
static gtime_Timer* gtime_bindings__get_timer(lua_State* lua, gtime_bindings__TimeBindings* ctx, int index);
static gtime_Stopwatch* gtime_bindings__get_stopwatch(lua_State* lua, gtime_bindings__TimeBindings* ctx, int index);
static gtime_bindings__TimeObject* gtime_bindings__push_object(
    galloc_Allocator alloc,
    lua_State* lua,
    const char* metatable
);
static gecs_ID gtime_bindings__check_entity(lua_State* lua, gtime_bindings__TimeBindings* ctx, int index);
static gtime_TimerMode gtime_bindings__check_mode(lua_State* lua, int index);
static float gtime_bindings__delta_time(lua_State* lua, gtime_bindings__TimeBindings* ctx);
static int gtime_bindings__timer_new(lua_State* lua);
static int gtime_bindings__stopwatch_new(lua_State* lua);
static int gtime_bindings__timer_set(lua_State* lua);
static int gtime_bindings__stopwatch_set(lua_State* lua);
static int gtime_bindings__component_get(lua_State* lua, bool timer);
static int gtime_bindings__component_has(lua_State* lua, bool timer);
static int gtime_bindings__component_remove(lua_State* lua, bool timer);
static int gtime_bindings__timer_get(lua_State* lua);
static int gtime_bindings__timer_has(lua_State* lua);
static int gtime_bindings__timer_remove(lua_State* lua);
static int gtime_bindings__stopwatch_get(lua_State* lua);
static int gtime_bindings__stopwatch_has(lua_State* lua);
static int gtime_bindings__stopwatch_remove(lua_State* lua);
static int gtime_bindings__timer_index(lua_State* lua);
static int gtime_bindings__timer_newindex(lua_State* lua);
static int gtime_bindings__stopwatch_index(lua_State* lua);
static int gtime_bindings__stopwatch_newindex(lua_State* lua);
static int gtime_bindings__timer_tick(lua_State* lua);
static int gtime_bindings__stopwatch_tick(lua_State* lua);
static int gtime_bindings__stopwatch_is_paused(lua_State* lua);
static void gtime_bindings__set_function(
    galloc_Allocator alloc,
    lua_State* lua,
    int context,
    const char* name,
    lua_CFunction function
);
static void gtime_bindings__register_timer(galloc_Allocator alloc, lua_State* lua, int context);
static void gtime_bindings__register_stopwatch(galloc_Allocator alloc, lua_State* lua, int context);

static int gtime_bindings__lua_timer_reset(lua_State* lua);
static int gtime_bindings__lua_timer_pause(lua_State* lua);
static int gtime_bindings__lua_timer_unpause(lua_State* lua);
static int gtime_bindings__lua_timer_finish(lua_State* lua);
static int gtime_bindings__lua_timer_almost_finish(lua_State* lua);
static int gtime_bindings__lua_stopwatch_reset(lua_State* lua);
static int gtime_bindings__lua_stopwatch_pause(lua_State* lua);
static int gtime_bindings__lua_stopwatch_unpause(lua_State* lua);

bool gtime_bindings_bind(lua_State* lua, gecs_World* world, gtime_FrameTimeFn frame_time, void* frame_time_user) {
    if (lua == NULL || world == NULL) {
        return false;
    }

    galloc_Allocator alloc = glint_allocator(world);

    gecs_Component timer = GECS_COMPONENT(world, gtime_Timer);
    gecs_Component stopwatch = GECS_COMPONENT(world, gtime_Stopwatch);
    if (timer == 0 || stopwatch == 0) {
        return false;
    }

    gtime_bindings__TimeBindings* ctx = lua_newuserdatauv(lua, sizeof(*ctx), 0);
    *ctx = (gtime_bindings__TimeBindings) {
        .alloc = alloc,
        .world = world,
        .timer = timer,
        .stopwatch = stopwatch,
        .frame_time = frame_time,
        .frame_time_user = frame_time_user
    };
    int context = lua_gettop(lua);
    gtime_bindings__register_timer(alloc, lua, context);
    gtime_bindings__register_stopwatch(alloc, lua, context);
    lua_newtable(lua);
    lua_pushinteger(lua, GTIME_TIMER_ONCE);
    lua_setfield(lua, -2, "once");
    lua_pushinteger(lua, GTIME_TIMER_REPEATING);
    lua_setfield(lua, -2, "repeating");
    lua_setglobal(lua, "TimerMode");
    lua_pop(lua, 1);
    return true;
}

bool gtime_bindings_install(lua_State* lua, gecs_World* world, gtime_FrameTimeFn frame_time, void* user) {
    if (lua == NULL || world == NULL) {
        return false;
    }

    gtime_bindings__BindArgs args = {.world = world, .frame_time = frame_time, .user = user};
    lua_pushcfunction(lua, gtime_bindings__install);
    lua_pushlightuserdata(lua, &args);
    return lua_pcall(lua, 1, 0, 0) == LUA_OK;
}

static int gtime_bindings__install(lua_State* lua) {
    gtime_bindings__BindArgs* args = lua_touserdata(lua, 1);
    if (!gtime_bindings_bind(lua, args->world, args->frame_time, args->user)) {
        return luaL_error(lua, "could not register time components");
    }

    return 0;
}

static gtime_bindings__TimeBindings* gtime_bindings__bindings(lua_State* lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static gtime_Timer* gtime_bindings__get_timer(lua_State* lua, gtime_bindings__TimeBindings* ctx, int index) {
    gtime_bindings__TimeObject* object = luaL_checkudata(lua, index, GTIME_BINDINGS_TIMER_META);
    if (object->world == NULL) {
        return &object->value.timer;
    }

    (void)ctx;

    gtime_Timer* timer =
        gecs_Component_get((gecs_Entity) {.world = object->world, .id = object->entity}, object->bindings->timer);
    if (timer == NULL) {
        luaL_error(lua, "Timer component no longer exists");
    }

    return timer;
}

static gtime_Stopwatch* gtime_bindings__get_stopwatch(lua_State* lua, gtime_bindings__TimeBindings* ctx, int index) {
    gtime_bindings__TimeObject* object = luaL_checkudata(lua, index, GTIME_BINDINGS_STOPWATCH_META);
    if (object->world == NULL) {
        return &object->value.stopwatch;
    }

    (void)ctx;

    gtime_Stopwatch* stopwatch =
        gecs_Component_get((gecs_Entity) {.world = object->world, .id = object->entity}, object->bindings->stopwatch);
    if (stopwatch == NULL) {
        luaL_error(lua, "Stopwatch component no longer exists");
    }

    return stopwatch;
}

static gtime_bindings__TimeObject* gtime_bindings__push_object(
    galloc_Allocator alloc,
    lua_State* lua,
    const char* metatable
) {
    (void)alloc;
    gtime_bindings__TimeObject* object = lua_newuserdatauv(lua, sizeof(*object), 1);
    memset(object, 0, sizeof(*object));
    // Keep the original binding alive even if a later bind replaces the
    // shared metatable's closures and the global constructor tables.
    object->bindings = gtime_bindings__bindings(lua);
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_setiuservalue(lua, -2, 1);
    luaL_setmetatable(lua, metatable);
    return object;
}

static gecs_ID gtime_bindings__check_entity(lua_State* lua, gtime_bindings__TimeBindings* ctx, int index) {
    gecs_Entity handle = gscript_bindings_check_entity(lua, index);
    if (handle.world != ctx->world || !gecs_Entity_alive(handle)) {
        luaL_error(lua, "Entity is invalid or belongs to a different world");
    }

    return handle.id;
}

static gtime_TimerMode gtime_bindings__check_mode(lua_State* lua, int index) {
    lua_Integer mode = luaL_checkinteger(lua, index);
    if (mode != GTIME_TIMER_ONCE && mode != GTIME_TIMER_REPEATING) {
        luaL_argerror(lua, index, "invalid timer mode");
    }

    return (gtime_TimerMode)mode;
}

static float gtime_bindings__delta_time(lua_State* lua, gtime_bindings__TimeBindings* ctx) {
    if (lua_isnoneornil(lua, 2) == 0) {
        return (float)luaL_checknumber(lua, 2);
    }

    return ctx->frame_time != NULL ? ctx->frame_time(ctx->frame_time_user) : 0.0f;
}

static int gtime_bindings__timer_new(lua_State* lua) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    float duration = (float)luaL_checknumber(lua, 1);
    gtime_TimerMode mode = gtime_bindings__check_mode(lua, 2);
    gtime_bindings__TimeObject* object = gtime_bindings__push_object(ctx->alloc, lua, GTIME_BINDINGS_TIMER_META);
    object->value.timer = gtime_Timer_new(duration, mode);
    return 1;
}

static int gtime_bindings__stopwatch_new(lua_State* lua) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gtime_bindings__TimeObject* object = gtime_bindings__push_object(ctx->alloc, lua, GTIME_BINDINGS_STOPWATCH_META);
    object->value.stopwatch = gtime_Stopwatch_new();
    return 1;
}

static int gtime_bindings__timer_set(lua_State* lua) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gecs_ID entity = gtime_bindings__check_entity(lua, ctx, 1);
    gtime_Timer value;
    if (luaL_testudata(lua, 2, GTIME_BINDINGS_TIMER_META) != NULL) {
        value = *gtime_bindings__get_timer(lua, ctx, 2);
    } else {
        value = gtime_Timer_new((float)luaL_checknumber(lua, 2), gtime_bindings__check_mode(lua, 3));
    }

    if (gecs_Component_set((gecs_Entity) {.world = ctx->world, .id = entity}, ctx->timer, &value) == NULL) {
        return luaL_error(lua, "could not allocate Timer component");
    }

    gtime_bindings__TimeObject* object = gtime_bindings__push_object(ctx->alloc, lua, GTIME_BINDINGS_TIMER_META);
    object->world = ctx->world;
    object->entity = entity;
    return 1;
}

static int gtime_bindings__stopwatch_set(lua_State* lua) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gecs_ID entity = gtime_bindings__check_entity(lua, ctx, 1);
    gtime_Stopwatch value = gtime_Stopwatch_new();
    if (lua_isnoneornil(lua, 2) == 0) {
        value = *gtime_bindings__get_stopwatch(lua, ctx, 2);
    }

    if (gecs_Component_set((gecs_Entity) {.world = ctx->world, .id = entity}, ctx->stopwatch, &value) == NULL) {
        return luaL_error(lua, "could not allocate Stopwatch component");
    }

    gtime_bindings__TimeObject* object = gtime_bindings__push_object(ctx->alloc, lua, GTIME_BINDINGS_STOPWATCH_META);
    object->world = ctx->world;
    object->entity = entity;
    return 1;
}

static int gtime_bindings__component_get(lua_State* lua, bool timer) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gecs_ID entity = gtime_bindings__check_entity(lua, ctx, 1);
    gecs_Component component = timer ? ctx->timer : ctx->stopwatch;
    if (gecs_Component_get((gecs_Entity) {.world = ctx->world, .id = entity}, component) == NULL) {
        return luaL_error(lua, "%s component does not exist", timer ? "Timer" : "Stopwatch");
    }

    gtime_bindings__TimeObject* object =
        gtime_bindings__push_object(ctx->alloc, lua, timer ? GTIME_BINDINGS_TIMER_META : GTIME_BINDINGS_STOPWATCH_META);
    object->world = ctx->world;
    object->entity = entity;
    return 1;
}

static int gtime_bindings__component_has(lua_State* lua, bool timer) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gecs_ID entity = gtime_bindings__check_entity(lua, ctx, 1);
    gecs_Component component = timer ? ctx->timer : ctx->stopwatch;
    lua_pushboolean(lua, gecs_Component_get((gecs_Entity) {.world = ctx->world, .id = entity}, component) != NULL);
    return 1;
}

static int gtime_bindings__component_remove(lua_State* lua, bool timer) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gecs_ID entity = gtime_bindings__check_entity(lua, ctx, 1);
    gecs_Component_remove((gecs_Entity) {.world = ctx->world, .id = entity}, timer ? ctx->timer : ctx->stopwatch);
    return 0;
}

static int gtime_bindings__timer_get(lua_State* lua) {
    return gtime_bindings__component_get(lua, true);
}

static int gtime_bindings__timer_has(lua_State* lua) {
    return gtime_bindings__component_has(lua, true);
}

static int gtime_bindings__timer_remove(lua_State* lua) {
    return gtime_bindings__component_remove(lua, true);
}

static int gtime_bindings__stopwatch_get(lua_State* lua) {
    return gtime_bindings__component_get(lua, false);
}

static int gtime_bindings__stopwatch_has(lua_State* lua) {
    return gtime_bindings__component_has(lua, false);
}

static int gtime_bindings__stopwatch_remove(lua_State* lua) {
    return gtime_bindings__component_remove(lua, false);
}

static int gtime_bindings__timer_index(lua_State* lua) {
    gtime_bindings__TimeBindings* ctx = gtime_bindings__bindings(lua);
    gtime_Timer* timer = gtime_bindings__get_timer(lua, ctx, 1);
    const char* key = luaL_checkstring(lua, 2);
    if (strcmp(key, "duration") == 0) {
        lua_pushnumber(lua, timer->duration);
    } else if (strcmp(key, "mode") == 0) {
        lua_pushinteger(lua, timer->mode);
    } else if (strcmp(key, "elapsed") == 0) {
        lua_pushnumber(lua, timer->stopwatch.elapsed);
    } else if (strcmp(key, "times_finished_this_tick") == 0) {
        lua_pushinteger(lua, timer->times_finished_this_tick);
    } else if (strcmp(key, "is_finished") == 0) {
        lua_pushboolean(lua, timer->is_finished);
    } else if (strcmp(key, "just_finished") == 0) {
        lua_pushboolean(lua, gtime_Timer_just_finished(timer));
    } else if (strcmp(key, "is_paused") == 0) {
        lua_pushboolean(lua, gtime_Timer_is_paused(timer));
    } else if (strcmp(key, "remaining") == 0) {
        lua_pushnumber(lua, gtime_Timer_remaining(timer));
    } else if (strcmp(key, "fraction") == 0) {
        lua_pushnumber(lua, gtime_Timer_fraction(timer));
    } else {
        luaL_getmetatable(lua, GTIME_BINDINGS_TIMER_META);
        lua_getfield(lua, -1, key);
    }

    return 1;
}

static int gtime_bindings__timer_newindex(lua_State* lua) {
    gtime_Timer* timer = gtime_bindings__get_timer(lua, gtime_bindings__bindings(lua), 1);
    const char* key = luaL_checkstring(lua, 2);
    if (strcmp(key, "duration") == 0) {
        timer->duration = (float)luaL_checknumber(lua, 3);
    } else if (strcmp(key, "mode") == 0) {
        timer->mode = gtime_bindings__check_mode(lua, 3);
    } else if (strcmp(key, "elapsed") == 0) {
        timer->stopwatch.elapsed = (float)luaL_checknumber(lua, 3);
    } else {
        return luaL_error(lua, "Timer field '%s' is read-only or unknown", key);
    }

    return 0;
}

static int gtime_bindings__stopwatch_index(lua_State* lua) {
    gtime_Stopwatch* stopwatch = gtime_bindings__get_stopwatch(lua, gtime_bindings__bindings(lua), 1);
    const char* key = luaL_checkstring(lua, 2);
    if (strcmp(key, "elapsed") == 0) {
        lua_pushnumber(lua, stopwatch->elapsed);
    } else {
        luaL_getmetatable(lua, GTIME_BINDINGS_STOPWATCH_META);
        lua_getfield(lua, -1, key);
    }

    return 1;
}

static int gtime_bindings__stopwatch_newindex(lua_State* lua) {
    gtime_Stopwatch* stopwatch = gtime_bindings__get_stopwatch(lua, gtime_bindings__bindings(lua), 1);
    const char* key = luaL_checkstring(lua, 2);
    if (strcmp(key, "elapsed") != 0) {
        return luaL_error(lua, "unknown Stopwatch field '%s'", key);
    }

    stopwatch->elapsed = (float)luaL_checknumber(lua, 3);
    return 0;
}

static int gtime_bindings__timer_tick(lua_State* lua) {
    gtime_bindings__TimeObject* object = luaL_checkudata(lua, 1, GTIME_BINDINGS_TIMER_META);
    gtime_bindings__TimeBindings* ctx = object->bindings;
    gtime_Timer* timer = gtime_bindings__get_timer(lua, ctx, 1);
    gtime_Timer_tick(timer, gtime_bindings__delta_time(lua, ctx));
    lua_settop(lua, 1);
    return 1;
}

static int gtime_bindings__stopwatch_tick(lua_State* lua) {
    gtime_bindings__TimeObject* object = luaL_checkudata(lua, 1, GTIME_BINDINGS_STOPWATCH_META);
    gtime_bindings__TimeBindings* ctx = object->bindings;
    gtime_Stopwatch* stopwatch = gtime_bindings__get_stopwatch(lua, ctx, 1);
    gtime_Stopwatch_tick(stopwatch, gtime_bindings__delta_time(lua, ctx));
    lua_settop(lua, 1);
    return 1;
}

static int gtime_bindings__stopwatch_is_paused(lua_State* lua) {
    lua_pushboolean(lua, gtime_bindings__get_stopwatch(lua, gtime_bindings__bindings(lua), 1)->is_paused);
    return 1;
}

static void gtime_bindings__set_function(
    galloc_Allocator alloc,
    lua_State* lua,
    int context,
    const char* name,
    lua_CFunction function
) {
    (void)alloc;
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

static void gtime_bindings__register_timer(galloc_Allocator alloc, lua_State* lua, int context) {
    luaL_newmetatable(lua, GTIME_BINDINGS_TIMER_META);
    gtime_bindings__set_function(alloc, lua, context, "__index", gtime_bindings__timer_index);
    gtime_bindings__set_function(alloc, lua, context, "__newindex", gtime_bindings__timer_newindex);
    gtime_bindings__set_function(alloc, lua, context, "tick", gtime_bindings__timer_tick);
    gtime_bindings__set_function(alloc, lua, context, "reset", gtime_bindings__lua_timer_reset);
    gtime_bindings__set_function(alloc, lua, context, "pause", gtime_bindings__lua_timer_pause);
    gtime_bindings__set_function(alloc, lua, context, "unpause", gtime_bindings__lua_timer_unpause);
    gtime_bindings__set_function(alloc, lua, context, "finish", gtime_bindings__lua_timer_finish);
    gtime_bindings__set_function(alloc, lua, context, "almost_finish", gtime_bindings__lua_timer_almost_finish);
    lua_pop(lua, 1);
    lua_newtable(lua);
    gtime_bindings__set_function(alloc, lua, context, "new", gtime_bindings__timer_new);
    gtime_bindings__set_function(alloc, lua, context, "set", gtime_bindings__timer_set);
    gtime_bindings__set_function(alloc, lua, context, "get", gtime_bindings__timer_get);
    gtime_bindings__set_function(alloc, lua, context, "has", gtime_bindings__timer_has);
    gtime_bindings__set_function(alloc, lua, context, "remove", gtime_bindings__timer_remove);
    lua_setglobal(lua, "Timer");
}

static void gtime_bindings__register_stopwatch(galloc_Allocator alloc, lua_State* lua, int context) {
    luaL_newmetatable(lua, GTIME_BINDINGS_STOPWATCH_META);
    gtime_bindings__set_function(alloc, lua, context, "__index", gtime_bindings__stopwatch_index);
    gtime_bindings__set_function(alloc, lua, context, "__newindex", gtime_bindings__stopwatch_newindex);
    gtime_bindings__set_function(alloc, lua, context, "tick", gtime_bindings__stopwatch_tick);
    gtime_bindings__set_function(alloc, lua, context, "reset", gtime_bindings__lua_stopwatch_reset);
    gtime_bindings__set_function(alloc, lua, context, "pause", gtime_bindings__lua_stopwatch_pause);
    gtime_bindings__set_function(alloc, lua, context, "unpause", gtime_bindings__lua_stopwatch_unpause);
    gtime_bindings__set_function(alloc, lua, context, "is_paused", gtime_bindings__stopwatch_is_paused);
    lua_pop(lua, 1);
    lua_newtable(lua);
    gtime_bindings__set_function(alloc, lua, context, "new", gtime_bindings__stopwatch_new);
    gtime_bindings__set_function(alloc, lua, context, "set", gtime_bindings__stopwatch_set);
    gtime_bindings__set_function(alloc, lua, context, "get", gtime_bindings__stopwatch_get);
    gtime_bindings__set_function(alloc, lua, context, "has", gtime_bindings__stopwatch_has);
    gtime_bindings__set_function(alloc, lua, context, "remove", gtime_bindings__stopwatch_remove);
    lua_setglobal(lua, "Stopwatch");
}

#define GTIME_TIMER_METHOD(name) \
    static int gtime_bindings__lua_timer_##name(lua_State* lua) { \
        gtime_Timer_##name(gtime_bindings__get_timer(lua, gtime_bindings__bindings(lua), 1)); \
        lua_settop(lua, 1); \
        return 1; \
    }
GTIME_TIMER_METHOD(reset)
GTIME_TIMER_METHOD(pause)
GTIME_TIMER_METHOD(unpause)
GTIME_TIMER_METHOD(finish)
GTIME_TIMER_METHOD(almost_finish)
#undef GTIME_TIMER_METHOD

#define GTIME_STOPWATCH_METHOD(name) \
    static int gtime_bindings__lua_stopwatch_##name(lua_State* lua) { \
        gtime_Stopwatch_##name(gtime_bindings__get_stopwatch(lua, gtime_bindings__bindings(lua), 1)); \
        lua_settop(lua, 1); \
        return 1; \
    }
GTIME_STOPWATCH_METHOD(reset)
GTIME_STOPWATCH_METHOD(pause)
GTIME_STOPWATCH_METHOD(unpause)
#undef GTIME_STOPWATCH_METHOD

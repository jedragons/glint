#include <glint/time_lua.h>
#include <glint/time.h>
#include <glint/script_entity.h>

#include <lauxlib.h>
#include <lua.h>
#include <string.h>

#define GTIME_TIMER_META "glint.Timer"
#define GTIME_STOPWATCH_META "glint.Stopwatch"

typedef struct gtime__TimeBindings {
    galloc_Allocator alloc;
    gecs_World *world;
    gecs_Component timer;
    gecs_Component stopwatch;
    gtime_FrameTimeFn frame_time;
    void *frame_time_user;
} gtime__TimeBindings;

typedef struct gtime__TimeObject {
    gecs_Entity entity;
    gecs_World *world;
    gtime__TimeBindings *bindings;

    union {
        gtime_Timer timer;
        gtime_Stopwatch stopwatch;
    } value;
} gtime__TimeObject;

static gtime__TimeBindings *gtime__time_lua_time_bindings(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static gtime_Timer *gtime__time_lua_get_timer(lua_State *lua, gtime__TimeBindings *ctx, int index) {
    gtime__TimeObject *object = luaL_checkudata(lua, index, GTIME_TIMER_META);
    gtime_Timer *timer;
    if (!object->world)
        return &object->value.timer;
    (void)ctx;
    timer = gecs_Component_get(object->world, object->entity, object->bindings->timer);
    if (!timer)
        luaL_error(lua, "Timer component no longer exists");
    return timer;
}

static gtime_Stopwatch *gtime__time_lua_get_stopwatch(lua_State *lua, gtime__TimeBindings *ctx, int index) {
    gtime__TimeObject *object = luaL_checkudata(lua, index, GTIME_STOPWATCH_META);
    gtime_Stopwatch *stopwatch;
    if (!object->world)
        return &object->value.stopwatch;
    (void)ctx;
    stopwatch = gecs_Component_get(object->world, object->entity, object->bindings->stopwatch);
    if (!stopwatch)
        luaL_error(lua, "Stopwatch component no longer exists");
    return stopwatch;
}

static gtime__TimeObject *gtime__time_lua_push_object(galloc_Allocator alloc, lua_State *lua, const char *metatable) {
    gtime__TimeObject *object;
    /* Lua allocates this userdata through the state allocator. */
    (void)alloc;
    object = lua_newuserdatauv(lua, sizeof(*object), 1);
    memset(object, 0, sizeof(*object));
    /* Keep the original binding alive even if a later bind replaces the
     * shared metatable's closures and the global constructor tables. */
    object->bindings = gtime__time_lua_time_bindings(lua);
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_setiuservalue(lua, -2, 1);
    luaL_setmetatable(lua, metatable);
    return object;
}

static gecs_Entity gtime__time_lua_time_check_entity(lua_State *lua, gtime__TimeBindings *ctx, int index) {
    gecs_Entity entity = 0;
    gecs_World *world = NULL;
    gscript_check_entity(lua, index, &world, &entity);
    if (world != ctx->world || !gecs_Entity_alive(world, entity)) {
        luaL_error(lua, "Entity is invalid or belongs to a different world");
    }
    return entity;
}

static gtime_TimerMode gtime__time_lua_check_mode(lua_State *lua, int index) {
    lua_Integer mode = luaL_checkinteger(lua, index);
    if (mode != GTIME_TIMER_ONCE && mode != GTIME_TIMER_REPEATING) {
        luaL_argerror(lua, index, "invalid timer mode");
    }
    return (gtime_TimerMode)mode;
}

static float gtime__time_lua_delta_time(lua_State *lua, gtime__TimeBindings *ctx) {
    if (!lua_isnoneornil(lua, 2))
        return (float)luaL_checknumber(lua, 2);
    return ctx->frame_time ? ctx->frame_time(ctx->frame_time_user) : 0.0f;
}

static int gtime__time_lua_timer_new(lua_State *lua) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    float duration = (float)luaL_checknumber(lua, 1);
    gtime_TimerMode mode = gtime__time_lua_check_mode(lua, 2);
    gtime__TimeObject *object = gtime__time_lua_push_object(ctx->alloc, lua, GTIME_TIMER_META);
    object->value.timer = gtime_Timer_new(duration, mode);
    return 1;
}

static int gtime__time_lua_stopwatch_new(lua_State *lua) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gtime__TimeObject *object = gtime__time_lua_push_object(ctx->alloc, lua, GTIME_STOPWATCH_META);
    object->value.stopwatch = gtime_Stopwatch_new();
    return 1;
}

static int gtime__time_lua_timer_set(lua_State *lua) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gecs_Entity entity = gtime__time_lua_time_check_entity(lua, ctx, 1);
    gtime_Timer value;
    gtime__TimeObject *object;
    if (luaL_testudata(lua, 2, GTIME_TIMER_META)) {
        value = *gtime__time_lua_get_timer(lua, ctx, 2);
    } else {
        value = gtime_Timer_new((float)luaL_checknumber(lua, 2), gtime__time_lua_check_mode(lua, 3));
    }
    if (!gecs_Component_set(ctx->alloc, ctx->world, entity, ctx->timer, &value)) {
        return luaL_error(lua, "could not allocate Timer component");
    }
    object = gtime__time_lua_push_object(ctx->alloc, lua, GTIME_TIMER_META);
    object->world = ctx->world;
    object->entity = entity;
    return 1;
}

static int gtime__time_lua_stopwatch_set(lua_State *lua) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gecs_Entity entity = gtime__time_lua_time_check_entity(lua, ctx, 1);
    gtime_Stopwatch value = gtime_Stopwatch_new();
    gtime__TimeObject *object;
    if (!lua_isnoneornil(lua, 2))
        value = *gtime__time_lua_get_stopwatch(lua, ctx, 2);
    if (!gecs_Component_set(ctx->alloc, ctx->world, entity, ctx->stopwatch, &value)) {
        return luaL_error(lua, "could not allocate Stopwatch component");
    }
    object = gtime__time_lua_push_object(ctx->alloc, lua, GTIME_STOPWATCH_META);
    object->world = ctx->world;
    object->entity = entity;
    return 1;
}

static int gtime__time_lua_component_get(lua_State *lua, int timer) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gecs_Entity entity = gtime__time_lua_time_check_entity(lua, ctx, 1);
    gecs_Component component = timer ? ctx->timer : ctx->stopwatch;
    gtime__TimeObject *object;
    if (!gecs_Component_get(ctx->world, entity, component)) {
        return luaL_error(lua, "%s component does not exist", timer ? "Timer" : "Stopwatch");
    }
    object = gtime__time_lua_push_object(ctx->alloc, lua, timer ? GTIME_TIMER_META : GTIME_STOPWATCH_META);
    object->world = ctx->world;
    object->entity = entity;
    return 1;
}

static int gtime__time_lua_time_component_has(lua_State *lua, int timer) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gecs_Entity entity = gtime__time_lua_time_check_entity(lua, ctx, 1);
    gecs_Component component = timer ? ctx->timer : ctx->stopwatch;
    lua_pushboolean(lua, gecs_Component_get(ctx->world, entity, component) != NULL);
    return 1;
}

static int gtime__time_lua_time_component_remove(lua_State *lua, int timer) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gecs_Entity entity = gtime__time_lua_time_check_entity(lua, ctx, 1);
    gecs_Component_remove(ctx->world, entity, timer ? ctx->timer : ctx->stopwatch);
    return 0;
}

static int gtime__time_lua_timer_get(lua_State *lua) {
    return gtime__time_lua_component_get(lua, 1);
}

static int gtime__time_lua_timer_has(lua_State *lua) {
    return gtime__time_lua_time_component_has(lua, 1);
}

static int gtime__time_lua_timer_remove(lua_State *lua) {
    return gtime__time_lua_time_component_remove(lua, 1);
}

static int gtime__time_lua_stopwatch_get(lua_State *lua) {
    return gtime__time_lua_component_get(lua, 0);
}

static int gtime__time_lua_stopwatch_has(lua_State *lua) {
    return gtime__time_lua_time_component_has(lua, 0);
}

static int gtime__time_lua_stopwatch_remove(lua_State *lua) {
    return gtime__time_lua_time_component_remove(lua, 0);
}

static int gtime__time_lua_timer_index(lua_State *lua) {
    gtime__TimeBindings *ctx = gtime__time_lua_time_bindings(lua);
    gtime_Timer *timer = gtime__time_lua_get_timer(lua, ctx, 1);
    const char *key = luaL_checkstring(lua, 2);
    if (strcmp(key, "duration") == 0)
        lua_pushnumber(lua, timer->duration);
    else if (strcmp(key, "mode") == 0)
        lua_pushinteger(lua, timer->mode);
    else if (strcmp(key, "elapsed") == 0)
        lua_pushnumber(lua, timer->stopwatch.elapsed);
    else if (strcmp(key, "times_finished_this_tick") == 0)
        lua_pushinteger(lua, timer->times_finished_this_tick);
    else if (strcmp(key, "is_finished") == 0)
        lua_pushboolean(lua, timer->is_finished);
    else if (strcmp(key, "just_finished") == 0)
        lua_pushboolean(lua, gtime_Timer_just_finished(timer));
    else if (strcmp(key, "is_paused") == 0)
        lua_pushboolean(lua, gtime_Timer_is_paused(timer));
    else if (strcmp(key, "remaining") == 0)
        lua_pushnumber(lua, gtime_Timer_remaining(timer));
    else if (strcmp(key, "fraction") == 0)
        lua_pushnumber(lua, gtime_Timer_fraction(timer));
    else {
        luaL_getmetatable(lua, GTIME_TIMER_META);
        lua_getfield(lua, -1, key);
    }
    return 1;
}

static int gtime__time_lua_timer_newindex(lua_State *lua) {
    gtime_Timer *timer = gtime__time_lua_get_timer(lua, gtime__time_lua_time_bindings(lua), 1);
    const char *key = luaL_checkstring(lua, 2);
    if (strcmp(key, "duration") == 0)
        timer->duration = (float)luaL_checknumber(lua, 3);
    else if (strcmp(key, "mode") == 0)
        timer->mode = gtime__time_lua_check_mode(lua, 3);
    else if (strcmp(key, "elapsed") == 0)
        timer->stopwatch.elapsed = (float)luaL_checknumber(lua, 3);
    else
        return luaL_error(lua, "Timer field '%s' is read-only or unknown", key);
    return 0;
}

static int gtime__time_lua_stopwatch_index(lua_State *lua) {
    gtime_Stopwatch *stopwatch = gtime__time_lua_get_stopwatch(lua, gtime__time_lua_time_bindings(lua), 1);
    const char *key = luaL_checkstring(lua, 2);
    if (strcmp(key, "elapsed") == 0)
        lua_pushnumber(lua, stopwatch->elapsed);
    else {
        luaL_getmetatable(lua, GTIME_STOPWATCH_META);
        lua_getfield(lua, -1, key);
    }
    return 1;
}

static int gtime__time_lua_stopwatch_newindex(lua_State *lua) {
    gtime_Stopwatch *stopwatch = gtime__time_lua_get_stopwatch(lua, gtime__time_lua_time_bindings(lua), 1);
    const char *key = luaL_checkstring(lua, 2);
    if (strcmp(key, "elapsed") != 0)
        return luaL_error(lua, "unknown Stopwatch field '%s'", key);
    stopwatch->elapsed = (float)luaL_checknumber(lua, 3);
    return 0;
}

#define GTIME_TIMER_METHOD(name) \
    static int gtime__lua_timer_##name(lua_State *lua) { \
        gtime_Timer_##name(gtime__time_lua_get_timer(lua, gtime__time_lua_time_bindings(lua), 1)); \
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
    static int gtime__lua_stopwatch_##name(lua_State *lua) { \
        gtime_Stopwatch_##name(gtime__time_lua_get_stopwatch(lua, gtime__time_lua_time_bindings(lua), 1)); \
        lua_settop(lua, 1); \
        return 1; \
    }
GTIME_STOPWATCH_METHOD(reset)
GTIME_STOPWATCH_METHOD(pause)
GTIME_STOPWATCH_METHOD(unpause)
#undef GTIME_STOPWATCH_METHOD

static int gtime__time_lua_timer_tick(lua_State *lua) {
    gtime__TimeObject *object = luaL_checkudata(lua, 1, GTIME_TIMER_META);
    gtime__TimeBindings *ctx = object->bindings;
    gtime_Timer *timer = gtime__time_lua_get_timer(lua, ctx, 1);
    gtime_Timer_tick(timer, gtime__time_lua_delta_time(lua, ctx));
    lua_settop(lua, 1);
    return 1;
}

static int gtime__time_lua_stopwatch_tick(lua_State *lua) {
    gtime__TimeObject *object = luaL_checkudata(lua, 1, GTIME_STOPWATCH_META);
    gtime__TimeBindings *ctx = object->bindings;
    gtime_Stopwatch *stopwatch = gtime__time_lua_get_stopwatch(lua, ctx, 1);
    gtime_Stopwatch_tick(stopwatch, gtime__time_lua_delta_time(lua, ctx));
    lua_settop(lua, 1);
    return 1;
}

static int gtime__time_lua_stopwatch_is_paused(lua_State *lua) {
    lua_pushboolean(lua, gtime__time_lua_get_stopwatch(lua, gtime__time_lua_time_bindings(lua), 1)->is_paused);
    return 1;
}

static void gtime__time_lua_time_set_function(galloc_Allocator alloc, lua_State *lua, int context, const char *name, lua_CFunction function) {
    /* The closure is allocated through Lua's state allocator. */
    (void)alloc;
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

static void gtime__time_lua_register_timer(galloc_Allocator alloc, lua_State *lua, int context) {
    luaL_newmetatable(lua, GTIME_TIMER_META);
    gtime__time_lua_time_set_function(alloc, lua, context, "__index", gtime__time_lua_timer_index);
    gtime__time_lua_time_set_function(alloc, lua, context, "__newindex", gtime__time_lua_timer_newindex);
    gtime__time_lua_time_set_function(alloc, lua, context, "tick", gtime__time_lua_timer_tick);
    gtime__time_lua_time_set_function(alloc, lua, context, "reset", gtime__lua_timer_reset);
    gtime__time_lua_time_set_function(alloc, lua, context, "pause", gtime__lua_timer_pause);
    gtime__time_lua_time_set_function(alloc, lua, context, "unpause", gtime__lua_timer_unpause);
    gtime__time_lua_time_set_function(alloc, lua, context, "finish", gtime__lua_timer_finish);
    gtime__time_lua_time_set_function(alloc, lua, context, "almost_finish", gtime__lua_timer_almost_finish);
    lua_pop(lua, 1);
    lua_newtable(lua);
    gtime__time_lua_time_set_function(alloc, lua, context, "new", gtime__time_lua_timer_new);
    gtime__time_lua_time_set_function(alloc, lua, context, "set", gtime__time_lua_timer_set);
    gtime__time_lua_time_set_function(alloc, lua, context, "get", gtime__time_lua_timer_get);
    gtime__time_lua_time_set_function(alloc, lua, context, "has", gtime__time_lua_timer_has);
    gtime__time_lua_time_set_function(alloc, lua, context, "remove", gtime__time_lua_timer_remove);
    lua_setglobal(lua, "Timer");
}

static void gtime__time_lua_register_stopwatch(galloc_Allocator alloc, lua_State *lua, int context) {
    luaL_newmetatable(lua, GTIME_STOPWATCH_META);
    gtime__time_lua_time_set_function(alloc, lua, context, "__index", gtime__time_lua_stopwatch_index);
    gtime__time_lua_time_set_function(alloc, lua, context, "__newindex", gtime__time_lua_stopwatch_newindex);
    gtime__time_lua_time_set_function(alloc, lua, context, "tick", gtime__time_lua_stopwatch_tick);
    gtime__time_lua_time_set_function(alloc, lua, context, "reset", gtime__lua_stopwatch_reset);
    gtime__time_lua_time_set_function(alloc, lua, context, "pause", gtime__lua_stopwatch_pause);
    gtime__time_lua_time_set_function(alloc, lua, context, "unpause", gtime__lua_stopwatch_unpause);
    gtime__time_lua_time_set_function(alloc, lua, context, "is_paused", gtime__time_lua_stopwatch_is_paused);
    lua_pop(lua, 1);
    lua_newtable(lua);
    gtime__time_lua_time_set_function(alloc, lua, context, "new", gtime__time_lua_stopwatch_new);
    gtime__time_lua_time_set_function(alloc, lua, context, "set", gtime__time_lua_stopwatch_set);
    gtime__time_lua_time_set_function(alloc, lua, context, "get", gtime__time_lua_stopwatch_get);
    gtime__time_lua_time_set_function(alloc, lua, context, "has", gtime__time_lua_stopwatch_has);
    gtime__time_lua_time_set_function(alloc, lua, context, "remove", gtime__time_lua_stopwatch_remove);
    lua_setglobal(lua, "Stopwatch");
}

bool gtime_bind_lua(
    galloc_Allocator alloc,
    lua_State *lua,
    gecs_World *world,
    gtime_FrameTimeFn frame_time,
    void *frame_time_user
) {
    gecs_Component timer, stopwatch;
    gtime__TimeBindings *ctx;
    int context;
    if (!lua || !world)
        return false;
    timer = GECS_COMPONENT(alloc, world, gtime_Timer);
    stopwatch = GECS_COMPONENT(alloc, world, gtime_Stopwatch);
    if (!timer || !stopwatch)
        return false;
    ctx = lua_newuserdatauv(lua, sizeof(*ctx), 0);
    *ctx = (gtime__TimeBindings) {alloc, world, timer, stopwatch, frame_time, frame_time_user};
    context = lua_gettop(lua);
    gtime__time_lua_register_timer(alloc, lua, context);
    gtime__time_lua_register_stopwatch(alloc, lua, context);
    lua_newtable(lua);
    lua_pushinteger(lua, GTIME_TIMER_ONCE);
    lua_setfield(lua, -2, "once");
    lua_pushinteger(lua, GTIME_TIMER_REPEATING);
    lua_setfield(lua, -2, "repeating");
    lua_setglobal(lua, "TimerMode");
    lua_pop(lua, 1);
    return true;
}

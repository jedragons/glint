#include <glint/types_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <stdint.h>
#include <string.h>

#define GVEC2_METATABLE "glint.Vec2"

typedef struct gtypes__TypesBindings {
    galloc_Allocator alloc;
} gtypes__TypesBindings;

typedef struct gtypes__NamedColor {
    const char *name;
    uint8_t r, g, b, a;
} gtypes__NamedColor;

static const gtypes__NamedColor gtypes__named_colors[] = {
    {"lightgray", 200, 200, 200, 255}, {"gray", 130, 130, 130, 255},  {"darkgray", 80, 80, 80, 255},
    {"yellow", 253, 249, 0, 255},      {"gold", 255, 203, 0, 255},    {"orange", 255, 161, 0, 255},
    {"pink", 255, 109, 194, 255},      {"red", 230, 41, 55, 255},     {"maroon", 190, 33, 55, 255},
    {"green", 0, 228, 48, 255},        {"lime", 0, 158, 47, 255},     {"darkgreen", 0, 117, 44, 255},
    {"skyblue", 102, 191, 255, 255},   {"blue", 0, 121, 241, 255},    {"darkblue", 0, 82, 172, 255},
    {"purple", 200, 122, 255, 255},    {"violet", 135, 60, 190, 255}, {"darkpurple", 112, 31, 126, 255},
    {"beige", 211, 176, 131, 255},     {"brown", 127, 106, 79, 255},  {"darkbrown", 76, 63, 47, 255},
    {"white", 255, 255, 255, 255},     {"black", 0, 0, 0, 255},       {"blank", 0, 0, 0, 0},
    {"magenta", 255, 0, 255, 255},
};

static galloc_Allocator gtypes__types_lua_types_allocator(lua_State *lua) {
    gtypes__TypesBindings *ctx = lua_touserdata(lua, lua_upvalueindex(1));
    return ctx->alloc;
}

static void gtypes__types_lua_types_number_field(galloc_Allocator alloc, lua_State *lua, const char *key, lua_Number value) {
    (void)alloc;
    lua_pushnumber(lua, value);
    lua_setfield(lua, -2, key);
}

static void gtypes__types_lua_types_integer_field(galloc_Allocator alloc, lua_State *lua, const char *key, lua_Integer value) {
    (void)alloc;
    lua_pushinteger(lua, value);
    lua_setfield(lua, -2, key);
}

void gtypes_push_vec2(galloc_Allocator alloc, lua_State *lua, float x, float y) {
    (void)alloc;
    lua_createtable(lua, 0, 2);
    gtypes__types_lua_types_number_field(alloc, lua, "x", x);
    gtypes__types_lua_types_number_field(alloc, lua, "y", y);
    luaL_setmetatable(lua, GVEC2_METATABLE);
}

static float gtypes__types_lua_types_get_number(galloc_Allocator alloc, lua_State *lua, int index, const char *field) {
    float result;
    (void)alloc;
    index = lua_absindex(lua, index);
    if (!lua_istable(lua, index) && !lua_isuserdata(lua, index))
        luaL_argerror(lua, index, "expected a Vec2");
    lua_getfield(lua, index, field);
    result = (float)luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    return result;
}

static int gtypes__types_lua_types_vec2_new(lua_State *lua) {
    float x = (float)luaL_optnumber(lua, 1, 0);
    float y = (float)luaL_optnumber(lua, 2, 0);
    gtypes_push_vec2(gtypes__types_lua_types_allocator(lua), lua, x, y);
    return 1;
}

typedef enum gtypes__Vec2Operation { GTYPES_VEC2_ADD, GTYPES_VEC2_SUBTRACT, GTYPES_VEC2_MULTIPLY, GTYPES_VEC2_DIVIDE } gtypes__Vec2Operation;

static int gtypes__types_lua_types_vec2_binary(galloc_Allocator alloc, lua_State *lua, gtypes__Vec2Operation operation) {
    float ax = gtypes__types_lua_types_get_number(alloc, lua, 1, "x"), ay = gtypes__types_lua_types_get_number(alloc, lua, 1, "y");
    float bx = gtypes__types_lua_types_get_number(alloc, lua, 2, "x"), by = gtypes__types_lua_types_get_number(alloc, lua, 2, "y");
    float x = 0, y = 0;
    switch (operation) {
        case GTYPES_VEC2_ADD:
            x = ax + bx;
            y = ay + by;
            break;
        case GTYPES_VEC2_SUBTRACT:
            x = ax - bx;
            y = ay - by;
            break;
        case GTYPES_VEC2_MULTIPLY:
            x = ax * bx;
            y = ay * by;
            break;
        case GTYPES_VEC2_DIVIDE:
            x = ax / bx;
            y = ay / by;
            break;
    }
    gtypes_push_vec2(alloc, lua, x, y);
    return 1;
}

static int gtypes__types_lua_types_vec2_add(lua_State *lua) {
    return gtypes__types_lua_types_vec2_binary(gtypes__types_lua_types_allocator(lua), lua, GTYPES_VEC2_ADD);
}

static int gtypes__types_lua_types_vec2_subtract(lua_State *lua) {
    return gtypes__types_lua_types_vec2_binary(gtypes__types_lua_types_allocator(lua), lua, GTYPES_VEC2_SUBTRACT);
}

static int gtypes__types_lua_types_vec2_multiply(lua_State *lua) {
    return gtypes__types_lua_types_vec2_binary(gtypes__types_lua_types_allocator(lua), lua, GTYPES_VEC2_MULTIPLY);
}

static int gtypes__types_lua_types_vec2_divide(lua_State *lua) {
    return gtypes__types_lua_types_vec2_binary(gtypes__types_lua_types_allocator(lua), lua, GTYPES_VEC2_DIVIDE);
}

static int gtypes__types_lua_types_vec2_scale(lua_State *lua) {
    galloc_Allocator alloc = gtypes__types_lua_types_allocator(lua);
    float x = gtypes__types_lua_types_get_number(alloc, lua, 1, "x"), y = gtypes__types_lua_types_get_number(alloc, lua, 1, "y");
    float scale = (float)luaL_checknumber(lua, 2);
    gtypes_push_vec2(alloc, lua, x * scale, y * scale);
    return 1;
}

static int gtypes__types_lua_types_rect_new(lua_State *lua) {
    galloc_Allocator alloc = gtypes__types_lua_types_allocator(lua);
    float x = (float)luaL_optnumber(lua, 1, 0), y = (float)luaL_optnumber(lua, 2, 0);
    float w = (float)luaL_optnumber(lua, 3, 0), h = (float)luaL_optnumber(lua, 4, 0);
    (void)alloc;
    lua_createtable(lua, 0, 4);
    gtypes__types_lua_types_number_field(alloc, lua, "x", x);
    gtypes__types_lua_types_number_field(alloc, lua, "y", y);
    gtypes__types_lua_types_number_field(alloc, lua, "w", w);
    gtypes__types_lua_types_number_field(alloc, lua, "h", h);
    return 1;
}

static uint8_t gtypes__types_lua_types_color_channel(lua_State *lua, int index) {
    lua_Integer value = luaL_checkinteger(lua, index);
    if (value < 0 || value > 255)
        luaL_argerror(lua, index, "expected color channel between 0 and 255");
    return (uint8_t)value;
}

static void gtypes__types_lua_types_push_color(galloc_Allocator alloc, lua_State *lua, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    (void)alloc;
    lua_createtable(lua, 0, 4);
    gtypes__types_lua_types_integer_field(alloc, lua, "r", r);
    gtypes__types_lua_types_integer_field(alloc, lua, "g", g);
    gtypes__types_lua_types_integer_field(alloc, lua, "b", b);
    gtypes__types_lua_types_integer_field(alloc, lua, "a", a);
}

static int gtypes__types_lua_types_color_new(lua_State *lua) {
    int count = lua_gettop(lua);
    uint8_t r = 0, g = 0, b = 0, a = 0;
    if (count == 1) {
        uint32_t hex = (uint32_t)luaL_checkinteger(lua, 1);
        r = (uint8_t)(hex >> 24);
        g = (uint8_t)(hex >> 16);
        b = (uint8_t)(hex >> 8);
        a = (uint8_t)hex;
    } else if (count == 3 || count == 4) {
        r = gtypes__types_lua_types_color_channel(lua, 1);
        g = gtypes__types_lua_types_color_channel(lua, 2);
        b = gtypes__types_lua_types_color_channel(lua, 3);
        a = count == 4 ? gtypes__types_lua_types_color_channel(lua, 4) : 255;
    } else if (count != 0) {
        return luaL_error(lua, "Color.new expects zero, one, three, or four arguments");
    }
    gtypes__types_lua_types_push_color(gtypes__types_lua_types_allocator(lua), lua, r, g, b, a);
    return 1;
}

static int gtypes__types_lua_types_color_index(lua_State *lua) {
    const char *name = luaL_checkstring(lua, 2);
    for (size_t i = 0; i < sizeof(gtypes__named_colors) / sizeof(gtypes__named_colors[0]); i++) {
        const gtypes__NamedColor *color = &gtypes__named_colors[i];
        if (strcmp(name, color->name) == 0) {
            gtypes__types_lua_types_push_color(gtypes__types_lua_types_allocator(lua), lua, color->r, color->g, color->b, color->a);
            return 1;
        }
    }
    lua_pushnil(lua);
    return 1;
}

static void gtypes__types_lua_types_set_function(galloc_Allocator alloc, lua_State *lua, int context, const char *name, lua_CFunction function) {
    (void)alloc;
    lua_pushvalue(lua, context);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

void gtypes_install_vec2_ops(galloc_Allocator alloc, lua_State *lua, int metatable) {
    metatable = lua_absindex(lua, metatable);
    gtypes__TypesBindings *ctx = lua_newuserdatauv(lua, sizeof(*ctx), 0);
    ctx->alloc = alloc;
    int context = lua_gettop(lua);
    lua_pushvalue(lua, metatable);
    gtypes__types_lua_types_set_function(alloc, lua, context, "__add", gtypes__types_lua_types_vec2_add);
    gtypes__types_lua_types_set_function(alloc, lua, context, "__sub", gtypes__types_lua_types_vec2_subtract);
    gtypes__types_lua_types_set_function(alloc, lua, context, "__mul", gtypes__types_lua_types_vec2_multiply);
    gtypes__types_lua_types_set_function(alloc, lua, context, "__div", gtypes__types_lua_types_vec2_divide);
    gtypes__types_lua_types_set_function(alloc, lua, context, "__vec2_scale", gtypes__types_lua_types_vec2_scale);
    lua_pop(lua, 2);
}

bool gtypes_bind_lua(galloc_Allocator alloc, lua_State *lua) {
    gtypes__TypesBindings *ctx;
    int context;
    if (!lua)
        return false;
    ctx = lua_newuserdatauv(lua, sizeof(*ctx), 0);
    ctx->alloc = alloc;
    context = lua_gettop(lua);
    luaL_newmetatable(lua, GVEC2_METATABLE);
    gtypes_install_vec2_ops(alloc, lua, -1);
    lua_newtable(lua);
    lua_getfield(lua, -2, "__vec2_scale");
    lua_setfield(lua, -2, "scale");
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);
    lua_newtable(lua);
    gtypes__types_lua_types_set_function(alloc, lua, context, "new", gtypes__types_lua_types_vec2_new);
    lua_setglobal(lua, "Vec2");
    lua_newtable(lua);
    gtypes__types_lua_types_set_function(alloc, lua, context, "new", gtypes__types_lua_types_rect_new);
    lua_setglobal(lua, "Rect");
    lua_newtable(lua);
    gtypes__types_lua_types_set_function(alloc, lua, context, "new", gtypes__types_lua_types_color_new);
    lua_newtable(lua);
    gtypes__types_lua_types_set_function(alloc, lua, context, "__index", gtypes__types_lua_types_color_index);
    lua_setmetatable(lua, -2);
    lua_setglobal(lua, "Color");
    lua_pop(lua, 1);
    return true;
}

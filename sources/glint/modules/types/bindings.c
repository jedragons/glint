#include "./bindings.h"

#include <glint/math/color.h>
#include <glint/math/vec2.h>

#include <lauxlib.h>
#include <lua.h>

#include <stdint.h>
#include <string.h>

#define GTYPES_BINDINGS_VEC2_META "glint.Vec2"

typedef struct gtypes_bindings__NamedColor {
    const char* name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} gtypes_bindings__NamedColor;

static int gtypes_bindings__install(lua_State* lua);
static const gtypes_bindings__NamedColor gtypes_bindings__named_colors[] = {
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

static void gtypes_bindings__push_vec2(lua_State* lua, float x, float y);
static void gtypes_bindings__number_field(lua_State* lua, const char* key, lua_Number value);
static void gtypes_bindings__integer_field(lua_State* lua, const char* key, lua_Integer value);
static float gtypes_bindings__get_number(lua_State* lua, int index, const char* field);
static int gtypes_bindings__vec2_new(lua_State* lua);
static int gtypes_bindings__vec2_add(lua_State* lua);
static int gtypes_bindings__vec2_subtract(lua_State* lua);
static int gtypes_bindings__vec2_multiply(lua_State* lua);
static int gtypes_bindings__vec2_divide(lua_State* lua);
static int gtypes_bindings__vec2_scale(lua_State* lua);
static int gtypes_bindings__vec2_length(lua_State* lua);
static int gtypes_bindings__vec2_normalize(lua_State* lua);
static gvec2_Vec2 gtypes_bindings__check_vec2(lua_State* lua, int index);
static int gtypes_bindings__rect_new(lua_State* lua);
static uint8_t gtypes_bindings__color_channel(lua_State* lua, int index);
static void gtypes_bindings__push_color(lua_State* lua, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
static int gtypes_bindings__color_new(lua_State* lua);
static int gtypes_bindings__color_index(lua_State* lua);
static void gtypes_bindings__set_function(lua_State* lua, const char* name, lua_CFunction function);

void gtypes_bindings_push_vec2(galloc_Allocator alloc, lua_State* lua, float x, float y) {
    (void)alloc;
    gtypes_bindings__push_vec2(lua, x, y);
}

void gtypes_bindings_install_vec2_ops(galloc_Allocator alloc, lua_State* lua, int metatable) {
    (void)alloc;
    metatable = lua_absindex(lua, metatable);
    lua_pushvalue(lua, metatable);
    gtypes_bindings__set_function(lua, "__add", gtypes_bindings__vec2_add);
    gtypes_bindings__set_function(lua, "__sub", gtypes_bindings__vec2_subtract);
    gtypes_bindings__set_function(lua, "__mul", gtypes_bindings__vec2_multiply);
    gtypes_bindings__set_function(lua, "__div", gtypes_bindings__vec2_divide);
    lua_newtable(lua);
    gtypes_bindings__set_function(lua, "scale", gtypes_bindings__vec2_scale);
    gtypes_bindings__set_function(lua, "length", gtypes_bindings__vec2_length);
    gtypes_bindings__set_function(lua, "normalize", gtypes_bindings__vec2_normalize);
    lua_setfield(lua, metatable, "__vec2_methods");
    lua_pop(lua, 1);
}

bool gtypes_bindings_bind(galloc_Allocator alloc, lua_State* lua) {
    if (lua == NULL) {
        return false;
    }

    luaL_newmetatable(lua, GTYPES_BINDINGS_VEC2_META);
    gtypes_bindings_install_vec2_ops(alloc, lua, -1);
    lua_getfield(lua, -1, "__vec2_methods");
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_newtable(lua);
    gtypes_bindings__set_function(lua, "new", gtypes_bindings__vec2_new);
    lua_setglobal(lua, "Vec2");

    lua_newtable(lua);
    gtypes_bindings__set_function(lua, "new", gtypes_bindings__rect_new);
    lua_setglobal(lua, "Rect");

    lua_newtable(lua);
    gtypes_bindings__set_function(lua, "new", gtypes_bindings__color_new);
    lua_newtable(lua);
    gtypes_bindings__set_function(lua, "__index", gtypes_bindings__color_index);
    lua_setmetatable(lua, -2);
    lua_setglobal(lua, "Color");
    return true;
}

bool gtypes_bindings_install(galloc_Allocator alloc, lua_State* lua) {
    if (lua == NULL) {
        return false;
    }

    lua_pushcfunction(lua, gtypes_bindings__install);
    lua_pushlightuserdata(lua, &alloc);
    return lua_pcall(lua, 1, 0, 0) == LUA_OK;
}

static int gtypes_bindings__install(lua_State* lua) {
    galloc_Allocator* alloc = lua_touserdata(lua, 1);
    if (!gtypes_bindings_bind(*alloc, lua)) {
        return luaL_error(lua, "could not register type bindings");
    }

    return 0;
}

static void gtypes_bindings__push_vec2(lua_State* lua, float x, float y) {
    lua_createtable(lua, 0, 2);
    gtypes_bindings__number_field(lua, "x", x);
    gtypes_bindings__number_field(lua, "y", y);
    luaL_setmetatable(lua, GTYPES_BINDINGS_VEC2_META);
}

static void gtypes_bindings__number_field(lua_State* lua, const char* key, lua_Number value) {
    lua_pushnumber(lua, value);
    lua_setfield(lua, -2, key);
}

static void gtypes_bindings__integer_field(lua_State* lua, const char* key, lua_Integer value) {
    lua_pushinteger(lua, value);
    lua_setfield(lua, -2, key);
}

static float gtypes_bindings__get_number(lua_State* lua, int index, const char* field) {
    float result;
    index = lua_absindex(lua, index);
    if (lua_istable(lua, index) == 0 && lua_isuserdata(lua, index) == 0) {
        luaL_argerror(lua, index, "expected a Vec2");
    }

    lua_getfield(lua, index, field);
    result = (float)luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    return result;
}

static int gtypes_bindings__vec2_new(lua_State* lua) {
    float x = (float)luaL_optnumber(lua, 1, 0);
    float y = (float)luaL_optnumber(lua, 2, 0);
    gtypes_bindings__push_vec2(lua, x, y);
    return 1;
}

static int gtypes_bindings__vec2_add(lua_State* lua) {
    gvec2_Vec2 a = gtypes_bindings__check_vec2(lua, 1);
    gvec2_Vec2 b = gtypes_bindings__check_vec2(lua, 2);
    gvec2_Vec2 result = gvec2_add(a, b);
    gtypes_bindings__push_vec2(lua, result.x, result.y);
    return 1;
}

static int gtypes_bindings__vec2_subtract(lua_State* lua) {
    gvec2_Vec2 a = gtypes_bindings__check_vec2(lua, 1);
    gvec2_Vec2 b = gtypes_bindings__check_vec2(lua, 2);
    gvec2_Vec2 result = gvec2_sub(a, b);
    gtypes_bindings__push_vec2(lua, result.x, result.y);
    return 1;
}

static int gtypes_bindings__vec2_multiply(lua_State* lua) {
    gvec2_Vec2 a = gtypes_bindings__check_vec2(lua, 1);
    gvec2_Vec2 b = gtypes_bindings__check_vec2(lua, 2);
    gvec2_Vec2 result = gvec2_mul(a, b);
    gtypes_bindings__push_vec2(lua, result.x, result.y);
    return 1;
}

static int gtypes_bindings__vec2_divide(lua_State* lua) {
    gvec2_Vec2 a = gtypes_bindings__check_vec2(lua, 1);
    gvec2_Vec2 b = gtypes_bindings__check_vec2(lua, 2);
    gvec2_Vec2 result = gvec2_div(a, b);
    gtypes_bindings__push_vec2(lua, result.x, result.y);
    return 1;
}

static int gtypes_bindings__vec2_scale(lua_State* lua) {
    gvec2_Vec2 value = gtypes_bindings__check_vec2(lua, 1);
    float scale = (float)luaL_checknumber(lua, 2);
    gvec2_Vec2 result = gvec2_scale(value, scale);
    gtypes_bindings__push_vec2(lua, result.x, result.y);
    return 1;
}

static int gtypes_bindings__vec2_length(lua_State* lua) {
    gvec2_Vec2 value = gtypes_bindings__check_vec2(lua, 1);
    lua_pushnumber(lua, gvec2_length(value));
    return 1;
}

static int gtypes_bindings__vec2_normalize(lua_State* lua) {
    gvec2_Vec2 value = gtypes_bindings__check_vec2(lua, 1);
    gvec2_Vec2 result;
    if (!gvec2_normalize(value, &result)) {
        lua_pushnil(lua);
        return 1;
    }

    gtypes_bindings__push_vec2(lua, result.x, result.y);
    return 1;
}

static gvec2_Vec2 gtypes_bindings__check_vec2(lua_State* lua, int index) {
    float x = gtypes_bindings__get_number(lua, index, "x");
    float y = gtypes_bindings__get_number(lua, index, "y");
    return (gvec2_Vec2) {.x = x, .y = y};
}

static int gtypes_bindings__rect_new(lua_State* lua) {
    float x = (float)luaL_optnumber(lua, 1, 0);
    float y = (float)luaL_optnumber(lua, 2, 0);
    float w = (float)luaL_optnumber(lua, 3, 0);
    float h = (float)luaL_optnumber(lua, 4, 0);
    lua_createtable(lua, 0, 4);
    gtypes_bindings__number_field(lua, "x", x);
    gtypes_bindings__number_field(lua, "y", y);
    gtypes_bindings__number_field(lua, "w", w);
    gtypes_bindings__number_field(lua, "h", h);
    return 1;
}

static uint8_t gtypes_bindings__color_channel(lua_State* lua, int index) {
    lua_Integer value = luaL_checkinteger(lua, index);
    if (value < 0 || value > 255) {
        luaL_argerror(lua, index, "expected color channel between 0 and 255");
    }

    return (uint8_t)value;
}

static void gtypes_bindings__push_color(lua_State* lua, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    lua_createtable(lua, 0, 4);
    gtypes_bindings__integer_field(lua, "r", r);
    gtypes_bindings__integer_field(lua, "g", g);
    gtypes_bindings__integer_field(lua, "b", b);
    gtypes_bindings__integer_field(lua, "a", a);
}

static int gtypes_bindings__color_new(lua_State* lua) {
    int count = lua_gettop(lua);
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    if (count == 1) {
        uint32_t hex = (uint32_t)luaL_checkinteger(lua, 1);
        gcolor_Color color = gcolor_from_hex(hex);
        r = color.r;
        g = color.g;
        b = color.b;
        a = color.a;
    } else if (count == 3 || count == 4) {
        r = gtypes_bindings__color_channel(lua, 1);
        g = gtypes_bindings__color_channel(lua, 2);
        b = gtypes_bindings__color_channel(lua, 3);
        a = count == 4 ? gtypes_bindings__color_channel(lua, 4) : 255;
    } else if (count != 0) {
        return luaL_error(lua, "Color.new expects zero, one, three, or four arguments");
    }

    gtypes_bindings__push_color(lua, r, g, b, a);
    return 1;
}

static int gtypes_bindings__color_index(lua_State* lua) {
    const char* name = luaL_checkstring(lua, 2);
    for (size_t i = 0; i < sizeof(gtypes_bindings__named_colors) / sizeof(gtypes_bindings__named_colors[0]); i++) {
        const gtypes_bindings__NamedColor* color = &gtypes_bindings__named_colors[i];
        if (strcmp(name, color->name) == 0) {
            gtypes_bindings__push_color(lua, color->r, color->g, color->b, color->a);
            return 1;
        }
    }

    lua_pushnil(lua);
    return 1;
}

static void gtypes_bindings__set_function(lua_State* lua, const char* name, lua_CFunction function) {
    lua_pushcfunction(lua, function);
    lua_setfield(lua, -2, name);
}

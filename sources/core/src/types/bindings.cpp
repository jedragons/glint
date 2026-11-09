#include <glint/core/types/bindings.hpp>

#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/math.hpp>

namespace glint::core {

auto bind_vector2(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<Vec2>(
        /*@
          ---@class Vec2
          ---@field x number
          ---@field y number
          ---@operator add(Vec2): Vec2
          ---@operator sub(Vec2): Vec2
          ---@operator mul(Vec2): Vec2
          ---@operator div(Vec2): Vec2
          Vec2 = {}
         */
        "Vec2",

        /*@
          ---@overload fun(x: number, y: number): Vec2
          ---@return Vec2
          function Vec2.new() end
         */
        sol::constructors<Vec2(), Vec2(float, float)>(),

        "x",
        &Vec2::x,

        "y",
        &Vec2::y,

        sol::meta_function::addition,
        [](const Vec2& a, const Vec2& b) -> Vec2 {
            return a + b;
        },

        sol::meta_function::subtraction,
        [](const Vec2& a, const Vec2& b) -> Vec2 {
            return a - b;
        },

        sol::meta_function::multiplication,
        [](const Vec2& a, const Vec2& b) -> Vec2 {
            return a * b;
        },

        sol::meta_function::division,
        [](const Vec2& a, const Vec2& b) -> Vec2 {
            return a / b;
        },

        /*@
          ---@param scale number
          ---@return Vec2
          function Vec2:scale(scale) end
         */
        "scale",
        [](const Vec2& a, float scale) -> Vec2 {
            return a.scale(scale);
        }
    );
}

auto bind_rectangle(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<Rect>(
        /*@
          ---@class Rect
          ---@field x number
          ---@field y number
          ---@field w number
          ---@field h number
          Rect = {}
        */
        "Rect",

        /*@
          ---@overload fun(x: number, y: number, w: number, h: number): Rect
          ---@return Rect
          function Rect.new() end
        */
        sol::constructors<Rect(), Rect(float, float, float, float)>(),

        "x",
        &Rect::x,

        "y",
        &Rect::y,

        "w",
        &Rect::w,

        "h",
        &Rect::h
    );
}

auto bind_color(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<Color>(
        /*@
          ---@class Color
          ---@field r integer
          ---@field g integer
          ---@field b integer
          ---@field a integer
          ---@field lightgray Color
          ---@field gray Color
          ---@field darkgray Color
          ---@field yellow Color
          ---@field gold Color
          ---@field orange Color
          ---@field pink Color
          ---@field red Color
          ---@field maroon Color
          ---@field green Color
          ---@field lime Color
          ---@field darkgreen Color
          ---@field skyblue Color
          ---@field blue Color
          ---@field darkblue Color
          ---@field purple Color
          ---@field violet Color
          ---@field darkpurple Color
          ---@field beige Color
          ---@field brown Color
          ---@field darkbrown Color
          ---@field white Color
          ---@field black Color
          ---@field blank Color
          ---@field magenta Color
          Color = {}
         */
        "Color",

        /*@
          ---@overload fun(hex: integer): Color
          ---@overload fun(r: integer, g: integer, b: integer): Color
          ---@overload fun(r: integer, g: integer, b: integer, a: integer): Color
          ---@return Color
          function Color.new() end
         */
        "new",
        sol::factories(
            []() -> Color {
                return {};
            },
            [](unsigned int hex) -> Color {
                return Color::from_hex(hex);
            },
            [](unsigned char r, unsigned char g, unsigned char b) -> Color {
                return {.r = r, .g = g, .b = b, .a = 255};
            },
            [](unsigned char r, unsigned char g, unsigned char b, unsigned char a) -> Color {
                return {.r = r, .g = g, .b = b, .a = a};
            }
        ),

        "r",
        &Color::r,

        "g",
        &Color::g,

        "b",
        &Color::b,

        "a",
        &Color::a,

        "lightgray",
        sol::property([]() -> Color {
            return LightGray;
        }),

        "gray",
        sol::property([]() -> Color {
            return Gray;
        }),

        "darkgray",
        sol::property([]() -> Color {
            return DarkGray;
        }),

        "yellow",
        sol::property([]() -> Color {
            return Yellow;
        }),

        "gold",
        sol::property([]() -> Color {
            return Gold;
        }),

        "orange",
        sol::property([]() -> Color {
            return Orange;
        }),

        "pink",
        sol::property([]() -> Color {
            return Pink;
        }),

        "red",
        sol::property([]() -> Color {
            return Red;
        }),

        "maroon",
        sol::property([]() -> Color {
            return Maroon;
        }),

        "green",
        sol::property([]() -> Color {
            return Green;
        }),

        "lime",
        sol::property([]() -> Color {
            return Lime;
        }),

        "darkgreen",
        sol::property([]() -> Color {
            return DarkGreen;
        }),

        "skyblue",
        sol::property([]() -> Color {
            return SkyBlue;
        }),

        "blue",
        sol::property([]() -> Color {
            return Blue;
        }),

        "darkblue",
        sol::property([]() -> Color {
            return DarkBlue;
        }),

        "purple",
        sol::property([]() -> Color {
            return Purple;
        }),

        "violet",
        sol::property([]() -> Color {
            return Violet;
        }),

        "darkpurple",
        sol::property([]() -> Color {
            return DarkPurple;
        }),

        "beige",
        sol::property([]() -> Color {
            return Beige;
        }),

        "brown",
        sol::property([]() -> Color {
            return Brown;
        }),

        "darkbrown",
        sol::property([]() -> Color {
            return DarkBrown;
        }),

        "white",
        sol::property([]() -> Color {
            return White;
        }),

        "black",
        sol::property([]() -> Color {
            return Black;
        }),

        "blank",
        sol::property([]() -> Color {
            return Blank;
        }),

        "magenta",
        sol::property([]() -> Color {
            return Magenta;
        })
    );
}

} // namespace glint::core

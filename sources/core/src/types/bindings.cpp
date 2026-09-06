#include <glint/core/types/bindings.hpp>

#include <entt/entity/registry.hpp>
#include <raylib.h>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_vector2(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<Vector2>(
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
        sol::constructors<Vector2(), Vector2(float, float)>(),

        "x",
        &Vector2::x,

        "y",
        &Vector2::y,

        sol::meta_function::addition,
        [](const Vector2& a, const Vector2& b) -> Vector2 { return {a.x + b.x, a.y + b.y}; },

        sol::meta_function::subtraction,
        [](const Vector2& a, const Vector2& b) -> Vector2 { return {a.x - b.x, a.y - b.y}; },

        sol::meta_function::multiplication,
        [](const Vector2& a, const Vector2& b) -> Vector2 { return {a.x * b.x, a.y * b.y}; },

        sol::meta_function::division,
        [](const Vector2& a, const Vector2& b) -> Vector2 { return {a.x / b.x, a.y / b.y}; },

        /*@
          ---@param scale number
          ---@return Vec2
          function Vec2:scale(scale) end
         */
        "scale",
        [](const Vector2& a, float scale) -> Vector2 { return {a.x * scale, a.y * scale}; }
    );
}

auto bind_rectangle(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<Rectangle>(
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
        sol::constructors<Rectangle(), Rectangle(float, float, float, float)>(),

        "x",
        &Rectangle::x,

        "y",
        &Rectangle::y,

        "w",
        &Rectangle::width,

        "h",
        &Rectangle::height
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
          ---@field raywhite Color
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
            []() -> Color { return {}; },
            [](unsigned int hex) -> Color { return GetColor(hex); },
            [](unsigned char r, unsigned char g, unsigned char b) -> Color { return {r, g, b, 255}; },
            [](unsigned char r, unsigned char g, unsigned char b, unsigned char a) -> Color { return {r, g, b, a}; }
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
        sol::property([]() -> Color { return LIGHTGRAY; }),

        "gray",
        sol::property([]() -> Color { return GRAY; }),

        "darkgray",
        sol::property([]() -> Color { return DARKGRAY; }),

        "yellow",
        sol::property([]() -> Color { return YELLOW; }),

        "gold",
        sol::property([]() -> Color { return GOLD; }),

        "orange",
        sol::property([]() -> Color { return ORANGE; }),

        "pink",
        sol::property([]() -> Color { return PINK; }),

        "red",
        sol::property([]() -> Color { return RED; }),

        "maroon",
        sol::property([]() -> Color { return MAROON; }),

        "green",
        sol::property([]() -> Color { return GREEN; }),

        "lime",
        sol::property([]() -> Color { return LIME; }),

        "darkgreen",
        sol::property([]() -> Color { return DARKGREEN; }),

        "skyblue",
        sol::property([]() -> Color { return SKYBLUE; }),

        "blue",
        sol::property([]() -> Color { return BLUE; }),

        "darkblue",
        sol::property([]() -> Color { return DARKBLUE; }),

        "purple",
        sol::property([]() -> Color { return PURPLE; }),

        "violet",
        sol::property([]() -> Color { return VIOLET; }),

        "darkpurple",
        sol::property([]() -> Color { return DARKPURPLE; }),

        "beige",
        sol::property([]() -> Color { return BEIGE; }),

        "brown",
        sol::property([]() -> Color { return BROWN; }),

        "darkbrown",
        sol::property([]() -> Color { return DARKBROWN; }),

        "white",
        sol::property([]() -> Color { return WHITE; }),

        "black",
        sol::property([]() -> Color { return BLACK; }),

        "blank",
        sol::property([]() -> Color { return BLANK; }),

        "magenta",
        sol::property([]() -> Color { return MAGENTA; }),

        "raywhite",
        sol::property([]() -> Color { return RAYWHITE; })
    );
}

} // namespace glint::core

#include <glint/core/bindings/vector2.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <raylib-cpp/Color.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_color(entt::registry& reg) -> void {
    auto& lua = get_lua(reg);

    lua.new_usertype<raylib::Color>(
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
        sol::constructors<
            raylib::Color(),
            raylib::Color(unsigned int hexValue),
            raylib::Color(unsigned char red, unsigned char green, unsigned char blue),
            raylib::Color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)>(),

        "r",
        &raylib::Color::r,

        "g",
        &raylib::Color::g,

        "b",
        &raylib::Color::b,

        "a",
        &raylib::Color::a,

        "lightgray",
        sol::property([]() -> raylib::Color { return LIGHTGRAY; }),

        "gray",
        sol::property([]() -> raylib::Color { return GRAY; }),

        "darkgray",
        sol::property([]() -> raylib::Color { return DARKGRAY; }),

        "yellow",
        sol::property([]() -> raylib::Color { return YELLOW; }),

        "gold",
        sol::property([]() -> raylib::Color { return GOLD; }),

        "orange",
        sol::property([]() -> raylib::Color { return ORANGE; }),

        "pink",
        sol::property([]() -> raylib::Color { return PINK; }),

        "red",
        sol::property([]() -> raylib::Color { return RED; }),

        "maroon",
        sol::property([]() -> raylib::Color { return MAROON; }),

        "green",
        sol::property([]() -> raylib::Color { return GREEN; }),

        "lime",
        sol::property([]() -> raylib::Color { return LIME; }),

        "darkgreen",
        sol::property([]() -> raylib::Color { return DARKGREEN; }),

        "skyblue",
        sol::property([]() -> raylib::Color { return SKYBLUE; }),

        "blue",
        sol::property([]() -> raylib::Color { return BLUE; }),

        "darkblue",
        sol::property([]() -> raylib::Color { return DARKBLUE; }),

        "purple",
        sol::property([]() -> raylib::Color { return PURPLE; }),

        "violet",
        sol::property([]() -> raylib::Color { return VIOLET; }),

        "darkpurple",
        sol::property([]() -> raylib::Color { return DARKPURPLE; }),

        "beige",
        sol::property([]() -> raylib::Color { return BEIGE; }),

        "brown",
        sol::property([]() -> raylib::Color { return BROWN; }),

        "darkbrown",
        sol::property([]() -> raylib::Color { return DARKBROWN; }),

        "white",
        sol::property([]() -> raylib::Color { return WHITE; }),

        "black",
        sol::property([]() -> raylib::Color { return BLACK; }),

        "blank",
        sol::property([]() -> raylib::Color { return BLANK; }),

        "magenta",
        sol::property([]() -> raylib::Color { return MAGENTA; }),

        "raywhite",
        sol::property([]() -> raylib::Color { return RAYWHITE; })
    );
}

} // namespace glint::core

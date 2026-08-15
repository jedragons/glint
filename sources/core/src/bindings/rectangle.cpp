#include <glint/core/bindings/rectangle.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <raylib-cpp/Rectangle.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_rectangle(entt::registry& reg) -> void {
    auto& lua = get_lua(reg);

    lua.new_usertype<raylib::Rectangle>(
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
        sol::constructors<raylib::Rectangle(), raylib::Rectangle(float, float, float, float)>(),

        "x",
        &raylib::Rectangle::x,

        "y",
        &raylib::Rectangle::y,

        "w",
        &raylib::Rectangle::width,

        "h",
        &raylib::Rectangle::height
    );
}

} // namespace glint::core

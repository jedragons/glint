#include <glint/core/bindings/vector2.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <raylib-cpp/Vector2.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_vector2(entt::registry& reg) -> void {
    auto& lua = get_lua(reg);

    lua.new_usertype<raylib::Vector2>(
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
        &raylib::Vector2::x,

        "y",
        &raylib::Vector2::y,

        sol::meta_function::addition,
        [](raylib::Vector2& a, raylib::Vector2& b) -> raylib::Vector2 { return a + b; },

        sol::meta_function::subtraction,
        [](raylib::Vector2& a, raylib::Vector2& b) -> raylib::Vector2 { return a - b; },

        sol::meta_function::multiplication,
        [](raylib::Vector2& a, raylib::Vector2& b) -> raylib::Vector2 { return a * b; },

        sol::meta_function::division,
        [](raylib::Vector2& a, raylib::Vector2& b) -> raylib::Vector2 { return a / b; },

        /*@
          ---@param scale number
          ---@return Vec2
          function Vec2:scale(scale) end
         */
        "scale",
        [](raylib::Vector2& a, float number) -> raylib::Vector2 { return a.Scale(number); }
    );
}

} // namespace glint::core

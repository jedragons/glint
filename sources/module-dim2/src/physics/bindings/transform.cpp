#include <glint/dim2/physics/bindings/transform.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/dim2/physics/transform.hpp>

namespace glint::dim2 {

auto bind_transform(entt::registry& registry) -> void {
    auto lua = core::get_lua(registry);

    lua.new_usertype<Transform2D>(
        /*@
          ---@class Transform2D
          ---@field position Vec2
          ---@field scale Vec2
          ---@field origin Vec2
          ---@field rotation number
          Transform2D = {}
         */
        "Transform2D",

        /*@
          ---@overload fun(position: Vec2, scale: Vec2, origin: Vec2, rotation: Vec2): Transform2D
          ---@return Transform2D
          function Transform2D.new() end
         */
        sol::constructors<Transform2D(), Transform2D(Vector2, Vector2, Vector2, float)>(),

        "position",
        &Transform2D::position,

        "scale",
        &Transform2D::scale,

        "origin",
        &Transform2D::origin,

        "rotation",
        &Transform2D::rotation,

        /*@
          ---@overload fun(e): Transform2D
          ---@overload fun(e, position: Vec2, scale: Vec2, origin: Vec2, rotation: number): Transform2D
          ---@param e Entity
          ---@param comp Transform2D
          ---@return Transform2D
          function Transform2D.set(e, comp) end
         */
        "set",
        sol::overload(
            [](entt::handle& e) -> Transform2D& { return e.emplace_or_replace<Transform2D>(); },
            [](entt::handle& e, Vector2& position, Vector2& scale, Vector2& origin, float rotation) -> Transform2D& {
                return e.emplace_or_replace<Transform2D>(position, scale, origin, rotation);
            },
            [](entt::handle& e, const Transform2D& transform) -> Transform2D& {
                return e.emplace_or_replace<Transform2D>(transform);
            }
        ),

        /*@
          ---@param e Entity
          ---@return Transform2D
          function Transform2D.get(e) end

          ---@param e Entity
          ---@return boolean
          function Transform2D.has(e) end

          ---@param e Entity
          function Transform2D.remove(e) end
         */
        GLINT_COMPONENT_FUNCS(Transform2D)
    );
}

} // namespace glint::dim2

#include <glint/dim2/bindings/animated_sprite.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/dim2/ecs/animated_sprite.hpp>

namespace glint::dim2 {

auto bind_animated_sprite(entt::registry& reg) -> void {
    auto& lua = core::get_lua(reg);

    lua.new_usertype<AnimatedSpriteComp>(
        /*@
          ---@class AnimatedSprite
          AnimatedSprite = {}
         */
        "AnimatedSprite",

        /*@
          ---@param atlas string
          ---@param name string
          ---@return AnimatedSprite
          function AnimatedSprite.load(atlas, name) end
         */
        "load",
        [&reg](const std::string& atlas, const std::string& sprite) -> AnimatedSpriteComp {
            return {load_atlas(reg, atlas)->animation(sprite)};
        },

        /*@
          ---@overload fun(e: Entity, atlas: string, name: string): AnimatedSprite
          ---@param e Entity
          ---@param animation AnimatedSprite
          ---@return AnimatedSprite
          function AnimatedSprite.set(e, animation) end
         */
        "set",
        sol::overload(
            [](entt::handle& e, const std::string& atlas, const std::string& animation) -> AnimatedSpriteComp& {
                return e.emplace_or_replace<AnimatedSpriteComp>(load_atlas(*e.registry(), atlas)->animation(animation));
            },
            [](entt::handle& e, const AnimatedSpriteComp& animation) -> AnimatedSpriteComp& {
                return e.emplace_or_replace<AnimatedSpriteComp>(animation);
            }
        ),

        /*@
          ---@param e Entity
          ---@return AnimatedSprite
          function AnimatedSprite.get(e) end

          ---@param e Entity
          ---@return boolean
          function AnimatedSprite.has(e) end

          ---@param e Entity
          function AnimatedSprite.remove(e) end
         */
        GLINT_COMPONENT_FUNCS(AnimatedSpriteComp)
    );
}

} // namespace glint::dim2

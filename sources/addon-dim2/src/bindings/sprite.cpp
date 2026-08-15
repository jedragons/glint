#include <glint/dim2/bindings/sprite.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/dim2/ecs/sprite.hpp>

namespace glint::dim2 {

auto bind_sprite(entt::registry& reg) -> void {
    auto& lua = core::get_lua(reg);

    lua.new_usertype<SpriteComp>(
        /*@
          ---@class Sprite
          Sprite = {}
         */
        "Sprite",

        /*@
          ---@param atlas string
          ---@param sprite string
          ---@return Sprite
          function Sprite.load(atlas, sprite) end
         */
        "load",
        [&reg](const std::string& atlas, const std::string& sprite) -> SpriteComp {
            return {load_atlas(reg, atlas)->sprite(sprite)};
        },

        /*@
          ---@overload fun(e: Entity, atlas: string, name: string): Sprite
          ---@param e Entity
          ---@param sprite Sprite
          ---@return Sprite
          function Sprite.set(e, sprite) end
         */
        "set",
        sol::overload(
            [&reg](entt::handle& e, const std::string& atlas, const std::string& sprite) -> SpriteComp& {
                return e.emplace_or_replace<SpriteComp>(load_atlas(reg, atlas)->sprite(sprite));
            },
            [](entt::handle& e, const SpriteComp& sprite) -> SpriteComp& {
                return e.emplace_or_replace<SpriteComp>(sprite);
            }
        ),

        /*@
          ---@param e Entity
          ---@return Sprite
          function Sprite.get(e) end

          ---@param e Entity
          ---@return boolean
          function Sprite.has(e) end

          ---@param e Entity
          function Sprite.remove(e) end
         */
        GLINT_COMPONENT_FUNCS(SpriteComp)
    );
}

} // namespace glint::dim2

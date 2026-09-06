#include <glint/dim2/rendering/bindings/atlas.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/dim2/rendering/ecs/animated_sprite.hpp>
#include <glint/dim2/rendering/ecs/atlas.hpp>
#include <glint/dim2/rendering/ecs/sprite.hpp>

namespace glint::dim2 {

auto bind_atlas(entt::registry& registry) -> void {
    auto lua = core::get_lua(registry);

    lua.new_usertype<AtlasComp>(
        /*@
          ---@class Atlas
          Atlas = {}
         */
        "Atlas",

        /*@
          ---@param path string
          ---@return Atlas
          function Atlas.load(path) end
         */
        "load",
        [&registry](const std::string& path) -> AtlasComp { return {load_atlas(registry, path)}; },

        /*@
          ---@param name string
          ---@return AnimatedSprite
          function Atlas:animation(name) end
         */
        "animation",
        [](AtlasComp& atlas, const std::string& name) -> AnimatedSpriteComp { return {atlas.atlas->animation(name)}; },

        /*@
          ---@param name string
          ---@return Sprite
          function Atlas:sprite(name) end
         */
        "sprite",
        [](AtlasComp& atlas, const std::string& name) -> SpriteComp { return {atlas.atlas->sprite(name)}; }
    );
}

} // namespace glint::dim2

#pragma once

#include <memory>

#include <entt/fwd.hpp>

#include <glint/dim2/rendering/asset/atlas.hpp>

namespace glint::dim2 {

struct AnimatedSpriteComp {
    std::shared_ptr<AnimatedSprite> sprite;
};

auto update_animated_sprites(entt::registry& registry) -> void;
auto draw_animated_sprites(entt::registry& registry) -> void;

} // namespace glint::dim2

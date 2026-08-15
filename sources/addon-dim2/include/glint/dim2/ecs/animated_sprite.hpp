#pragma once

#include <memory>

#include <entt/fwd.hpp>

#include <glint/dim2/asset/atlas.hpp>

namespace glint::dim2 {

struct AnimatedSpriteComp {
    std::shared_ptr<AnimatedSprite> sprite;
};

auto update_animated_sprites(entt::registry& reg) -> void;
auto draw_animated_sprites(entt::registry& reg) -> void;

} // namespace glint::dim2

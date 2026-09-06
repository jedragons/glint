#pragma once

#include <memory>

#include <entt/fwd.hpp>

#include <glint/dim2/rendering/asset/atlas.hpp>

namespace glint::dim2 {

struct SpriteComp {
    std::shared_ptr<Sprite> sprite;
};

auto draw_sprites(entt::registry& registry) -> void;

} // namespace glint::dim2

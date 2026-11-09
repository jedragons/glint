#include <glint/dim2/rendering/ecs/sprite.hpp>

#include <entt/entity/registry.hpp>

#include <glint/core/rendering.hpp>
#include <glint/dim2/physics/transform.hpp>

namespace glint::dim2 {

auto draw_sprites(entt::registry& registry) -> void {
    auto view = registry.view<SpriteComp, Transform2D>();
    auto& renderer = core::get_renderer(registry);

    for (auto [entity, sprite, transform] : view.each()) {
        auto dest = sprite.sprite->frame.rect;
        dest.x = transform.position.x;
        dest.y = transform.position.y;
        dest.w *= transform.scale.x;
        dest.h *= transform.scale.y;

        sprite.sprite->draw(renderer, dest, transform.origin, transform.rotation, White);
    }
}

} // namespace glint::dim2

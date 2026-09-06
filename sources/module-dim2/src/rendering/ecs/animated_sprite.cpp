#include <glint/dim2/rendering/ecs/animated_sprite.hpp>

#include <entt/entity/registry.hpp>

#include <glint/dim2/physics/transform.hpp>

namespace glint::dim2 {

auto update_animated_sprites(entt::registry& registry) -> void {
    auto view = registry.view<AnimatedSpriteComp>();
    for (auto [entity, sprite] : view.each()) {
        sprite.sprite->update();
    }
}

auto draw_animated_sprites(entt::registry& registry) -> void {
    auto view = registry.view<AnimatedSpriteComp, Transform2D>();
    for (auto [entity, sprite, transform] : view.each()) {
        auto frame = sprite.sprite->frame();
        if (!frame) continue;
        auto dest = frame->rect;
        dest.x = transform.position.x;
        dest.y = transform.position.y;
        dest.width *= transform.scale.x;
        dest.height *= transform.scale.y;

        sprite.sprite->draw(dest, transform.origin, transform.rotation, WHITE);
    }
}

} // namespace glint::dim2

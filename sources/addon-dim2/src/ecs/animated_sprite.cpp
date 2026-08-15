#include <glint/dim2/ecs/animated_sprite.hpp>

#include <glint/dim2/ecs/transform.hpp>

namespace glint::dim2 {

auto update_animated_sprites(entt::registry& reg) -> void {
    auto view = reg.view<AnimatedSpriteComp>();
    for (auto [entity, sprite] : view.each()) {
        sprite.sprite->update();
    }
}

auto draw_animated_sprites(entt::registry& reg) -> void {
    auto view = reg.view<AnimatedSpriteComp, Transform2D>();
    for (auto [entity, sprite, transform] : view.each()) {
        auto frame = sprite.sprite->frame();
        if (!frame) continue;
        auto src = frame->rect;
        auto dest = src;

        dest.SetPosition(transform.position);
        dest.width = dest.width * transform.scale.x;
        dest.height = dest.height * transform.scale.y;

        sprite.sprite->draw(dest, transform.origin, transform.rotation, WHITE);
    }
}

} // namespace glint::dim2

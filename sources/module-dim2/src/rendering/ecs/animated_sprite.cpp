#include <glint/dim2/rendering/ecs/animated_sprite.hpp>

#include <entt/entity/registry.hpp>

#include <glint/core/rendering.hpp>
#include <glint/core/windowing.hpp>
#include <glint/dim2/physics/transform.hpp>
#include <glint/platform.hpp>

namespace glint::dim2 {

auto update_animated_sprites(entt::registry& registry) -> void {
    auto view = registry.view<AnimatedSpriteComp>();
    auto& window = core::get_window(registry);
    for (auto [entity, sprite] : view.each()) {
        sprite.sprite->update(static_cast<float>(window.frame_time()));
    }
}

auto draw_animated_sprites(entt::registry& registry) -> void {
    auto view = registry.view<AnimatedSpriteComp, Transform2D>();
    auto& renderer = core::get_renderer(registry);
    for (auto [entity, sprite, transform] : view.each()) {
        auto frame = sprite.sprite->frame();
        if (!frame) continue;
        auto dest = frame->rect;
        dest.x = transform.position.x;
        dest.y = transform.position.y;
        dest.w *= transform.scale.x;
        dest.h *= transform.scale.y;

        sprite.sprite->draw(renderer, dest, transform.origin, transform.rotation, White);
    }
}

} // namespace glint::dim2

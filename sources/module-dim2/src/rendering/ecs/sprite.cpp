#include <glint/dim2/rendering/ecs/sprite.hpp>

#include <entt/entity/registry.hpp>

#include <glint/dim2/physics/transform.hpp>

namespace glint::dim2 {

auto draw_sprites(entt::registry& registry) -> void {
    auto view = registry.view<SpriteComp, Transform2D>();
    for (auto [entity, sprite, transform] : view.each()) {
        auto dest = sprite.sprite->frame.rect;
        dest.x = transform.position.x;
        dest.y = transform.position.y;
        dest.width *= transform.scale.x;
        dest.height *= transform.scale.y;

        sprite.sprite->draw(dest, transform.origin, transform.rotation, WHITE);
    }
}

} // namespace glint::dim2

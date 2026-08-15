#include <glint/dim2/ecs/sprite.hpp>

#include <glint/dim2/ecs/transform.hpp>

namespace glint::dim2 {

auto draw_sprites(entt::registry& reg) -> void {
    auto view = reg.view<SpriteComp, Transform2D>();
    for (auto [entity, sprite, transform] : view.each()) {
        auto src = sprite.sprite->frame.rect;
        auto dest = src;

        dest.SetPosition(transform.position);
        dest.width = dest.width * transform.scale.x;
        dest.height = dest.height * transform.scale.y;

        sprite.sprite->draw(dest, transform.origin, transform.rotation, WHITE);
    }
}

} // namespace glint::dim2

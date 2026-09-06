#include <glint/dim2/rendering.hpp>

#include <glint/core/lua.hpp>
#include <glint/core/texture.hpp>
#include <glint/dim2/physics.hpp>

namespace glint::dim2 {

static inline auto import_rendering(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    physics_module().import(registry);
    core::texture_module().import(registry);

    setup_atlases(registry);

    bind_animated_sprite(registry);
    bind_sprite(registry);
    bind_atlas(registry);
}

static inline auto update_rendering(entt::registry& registry) -> void {
    update_animated_sprites(registry);
}

static inline auto render_rendering(entt::registry& registry) -> void {
    draw_sprites(registry);
    draw_animated_sprites(registry);
}

auto rendering_module() -> Module {
    return Module {
        .name = "glint.dim2.rendering",
        .hooks = {
            .import = import_rendering,
            .update = update_rendering,
            .render = render_rendering,
        },
    };
}

} // namespace glint::dim2

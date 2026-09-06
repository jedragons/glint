#include <glint/nuklear.hpp>

#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include <raylib-nuklear.h>

#include <entt/entity/registry.hpp>

#include <glint/dim2/rendering.hpp>
#include <glint/nuklear/bindings/ui.hpp>

namespace glint::nuklear {

auto get_nuklear(entt::registry& registry) -> nk_context* {
    return registry.ctx().get<nk_context*>();
}

} // namespace glint::nuklear

namespace glint {

static inline auto import_nuklear(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    dim2::rendering_module().import(registry);

    auto* ctx = InitNuklear(32);
    registry.ctx().emplace<nk_context*>(ctx);

    nuklear::bind_ui(registry);
}

static inline auto unload_nuklear(entt::registry& registry) -> void {
    UnloadNuklear(nuklear::get_nuklear(registry));
    registry.ctx().erase<nk_context*>();
}

static inline auto update_nuklear(entt::registry& registry) -> void {
    UpdateNuklear(nuklear::get_nuklear(registry));
}

static inline auto render_nuklear(entt::registry& registry) -> void {
    DrawNuklear(nuklear::get_nuklear(registry));
}

auto nuklear_module() -> Module {
    return Module {
        .name = "glint.nuklear",
        .hooks = {
            .import = import_nuklear,
            .unload = unload_nuklear,
            .update = update_nuklear,
            .render = render_nuklear,
        },
    };
}

} // namespace glint

#include <glint/core/rendering.hpp>

#include <glint/core/lua.hpp>
#include <glint/platform.hpp>

namespace glint::core {

static auto import_rendering(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;
    lua_module().import(registry);
}

auto rendering_module() -> Module {
    return Module {
        .name = "glint.core.rendering",
        .hooks = {.import = import_rendering},
    };
}

auto set_renderer(entt::registry& reg, Renderer& renderer) -> void {
    reg.ctx().emplace<Renderer*>(&renderer);
}

auto get_renderer(entt::registry& reg) -> Renderer& {
    if (!reg.ctx().contains<Renderer*>()) {
        log_fatal("Window pointer was not set");
    }

    return *reg.ctx().get<Renderer*>();
}

} // namespace glint::core

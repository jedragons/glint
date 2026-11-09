#include <glint/core/windowing.hpp>

#include <glint/core/lua.hpp>
#include <glint/platform/log.hpp>
#include <glint/platform/window.hpp>

namespace glint::core {

static inline auto import_windowing(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    lua_module().import(registry);

    bind_window(registry);
}

auto windowing_module() -> Module {
    return Module {
        .name = "glint.core.windowing",
        .hooks = {.import = import_windowing},
    };
}

auto set_window(entt::registry& reg, Window& window) -> void {
    reg.ctx().emplace<Window*>(&window);
}

auto get_window(entt::registry& reg) -> Window& {
    if (!reg.ctx().contains<Window*>()) {
        log_fatal("Window pointer was not set");
    }

    return *reg.ctx().get<Window*>();
}

} // namespace glint::core

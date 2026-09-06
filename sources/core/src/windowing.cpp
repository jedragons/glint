#include <glint/core/windowing.hpp>

#include <glint/core/lua.hpp>

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

} // namespace glint::core

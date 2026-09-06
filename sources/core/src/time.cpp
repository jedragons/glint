#include <glint/core/time.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

static inline auto import_time(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    lua_module().import(registry);

    bind_stopwatch(registry);
    bind_timer(registry);
}

auto time_module() -> Module {
    return Module {
        .name = "glint.core.time",
        .hooks = {.import = import_time},
    };
}

} // namespace glint::core

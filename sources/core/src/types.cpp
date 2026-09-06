#include <glint/core/types.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

static inline auto import_types(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    lua_module().import(registry);

    bind_vector2(registry);
    bind_rectangle(registry);
    bind_color(registry);
}

auto types_module() -> Module {
    return Module {
        .name = "glint.core.types",
        .hooks = {.import = import_types},
    };
}

} // namespace glint::core

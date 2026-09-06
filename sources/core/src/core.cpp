#include <glint/core/core.hpp>

namespace glint {

static inline auto import_core(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    core::vfs_module().import(registry);
    core::windowing_module().import(registry);
    core::lua_module().import(registry);
    core::assets_module().import(registry);
    core::hierarchy_module().import(registry);
    core::time_module().import(registry);
    core::texture_module().import(registry);
    core::types_module().import(registry);
}

auto core_module() -> Module {
    return Module {
        .name = "glint.core",
        .hooks = {.import = import_core},
    };
}

} // namespace glint

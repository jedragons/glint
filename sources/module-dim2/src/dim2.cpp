#include <glint/dim2.hpp>

namespace glint {

static inline auto import_dim2(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    dim2::physics_module().import(registry);
    dim2::rendering_module().import(registry);
}

auto dim2_module() -> Module {
    return Module {
        .name = "glint.dim2",
        .hooks = {.import = import_dim2},
    };
}

} // namespace glint

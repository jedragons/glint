#include <glint/dim2/physics.hpp>

#include <glint/core/types.hpp>

namespace glint::dim2 {

static inline auto import_physics(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    core::types_module().import(registry);

    bind_transform(registry);
}

auto physics_module() -> Module {
    return Module {
        .name = "glint.dim2.physics",
        .hooks = {.import = import_physics},
    };
}

} // namespace glint::dim2

#include <glint/core/hierarchy.hpp>

#include <entt/entity/registry.hpp>

namespace glint::core {

static inline auto import_hierarchy(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    registry.ctx().emplace<NameMap>();

    registry.on_construct<NameComp>().connect<&NameComp::on_construct>();
    registry.on_update<NameComp>().connect<&NameComp::on_update>();
    registry.on_destroy<NameComp>().connect<&NameComp::on_destroy>();
}

auto hierarchy_module() -> Module {
    return Module {
        .name = "glint.core.hierarchy",
        .hooks = {.import = import_hierarchy},
    };
}

} // namespace glint::core

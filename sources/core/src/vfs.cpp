#include <glint/core/vfs.hpp>

#include <memory>

#include <entt/entity/registry.hpp>

namespace glint::core {

static inline auto import_vfs(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;
    registry.ctx().emplace<std::unique_ptr<vfs::Vfs>>(std::make_unique<vfs::Vfs>());
}

auto vfs_module() -> Module {
    return Module {
        .name = "glint.core.vfs",
        .hooks = {.import = import_vfs},
    };
}

auto get_vfs(entt::registry& registry) -> vfs::Vfs& {
    return *registry.ctx().get<std::unique_ptr<vfs::Vfs>>();
}

} // namespace glint::core

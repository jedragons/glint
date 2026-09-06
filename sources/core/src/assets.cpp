#include <glint/core/assets.hpp>

#include <entt/entity/registry.hpp>

#include <glint/core/vfs.hpp>

namespace glint::core {

static inline auto import_assets(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    vfs_module().import(registry);

#if GLINT_HOT_RELOAD_ASSETS
    registry.ctx().emplace<AssetWatcher>();
#endif
}

static inline auto update_assets(entt::registry& registry) -> void {
#if GLINT_HOT_RELOAD_ASSETS
    registry.ctx().get<AssetWatcher>().reload_pending_assets(registry);
#else
    (void)registry;
#endif
}

auto assets_module() -> Module {
    return Module {
        .name = "glint.core.assets",
        .hooks = {.import = import_assets, .update = update_assets},
    };
}

} // namespace glint::core

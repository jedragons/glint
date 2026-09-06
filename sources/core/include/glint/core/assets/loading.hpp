#pragma once

#include <memory>
#include <string>

#include <entt/entity/registry.hpp>
#include <raylib.h>

#include <glint/core/assets/asset.hpp>
#include <glint/core/assets/cache.hpp>
#include <glint/core/assets/resource.hpp>
#include <glint/core/assets/watcher.hpp>
#include <glint/core/vfs.hpp>

namespace glint::core {

template<is_asset Asset>
auto load_asset(entt::registry& registry, const std::string& path) -> Resource<Asset>;

template<is_asset Asset>
auto register_asset(entt::registry& registry) -> void;

} // namespace glint::core

// IMPLEMENTATION

namespace glint::core {

template<is_asset Asset>
auto load_asset(entt::registry& registry, const std::string& path) -> Resource<Asset> {
    auto& cache = registry.ctx().get<AssetCache<Asset>>();
    auto [asset, is_loaded] = cache.load(path, registry);

    if (is_loaded) {
        TraceLog(LOG_INFO, "[ASSET] %s loaded successfully", path.c_str());
#if GLINT_HOT_RELOAD_ASSETS
        auto& watcher = registry.ctx().get<AssetWatcher>();
        auto& vfs = registry.ctx().get<std::unique_ptr<vfs::Vfs>>();
        auto real_path = vfs->real_path(path);
        watcher.watch(real_path, path);
#endif
    }

    return asset;
}

template<is_asset Asset>
auto register_asset(entt::registry& registry) -> void {
    registry.ctx().emplace<AssetCache<Asset>>();

#if GLINT_HOT_RELOAD_ASSETS
    registry.ctx().get<AssetWatcher>().register_asset<Asset>(registry);
#endif
}

} // namespace glint::core

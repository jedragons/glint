#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <entt/entt.hpp>
#include <raylib-cpp/raylib.hpp>

#if GLINT_HOT_RELOAD_ASSETS
#include <wtr/watcher.hpp>
#endif

#include <glint/core/asset.hpp>
#include <glint/core/concurrent_queue.hpp>
#include <glint/core/vfs.hpp>

namespace glint::core {

template<is_asset Asset>
class AssetLoader {
  public:
    using result_type = entt::resource<Asset>;
    auto operator()(const std::string& path, entt::registry& reg) -> result_type;
};

template<is_asset Asset>
using AssetCache = entt::resource_cache<Asset, AssetLoader<Asset>>;

class AssetManager {
  public:
    AssetManager() = default;

    static auto register_path(entt::registry& reg, const std::string& path) -> void;

    template<is_asset Asset>
    static auto register_asset(entt::registry& reg) -> void;

    template<is_asset Asset>
    static auto load(entt::registry& reg, const std::string& path) -> entt::resource<Asset>;

    static auto update_assets(entt::registry& reg) -> void;

  private:
    using EventQueue = ConcurrentQueue<std::filesystem::path>;
    using EventHandler = std::function<void(const std::string&, entt::registry&)>;

    std::shared_ptr<EventQueue> m_event_queue = std::make_shared<EventQueue>();
    std::unordered_map<std::string, std::vector<EventHandler>> m_handlers {};
    std::unordered_map<std::string, std::string> m_path_map {};

#if GLINT_HOT_RELOAD_ASSETS
    using Watch = std::shared_ptr<wtr::watch>;
    std::vector<Watch> m_watchers {};
#endif
};

auto init_assets(entt::registry& reg) -> void;
auto update_assets(entt::registry& reg) -> void;

} // namespace glint::core

// IMPLEMENTATION

namespace glint::core {

template<is_asset Asset>
inline auto AssetLoader<Asset>::operator()(const std::string& path, entt::registry& reg) -> result_type {
    return entt::resource<Asset>(std::make_shared<Asset>(Asset::load(path, reg)));
}

template<is_asset Asset>
inline auto AssetManager::load(entt::registry& reg, const std::string& path) -> entt::resource<Asset> {
    auto id = entt::hashed_string {path.c_str(), path.size()};
    auto& cache = reg.ctx().get<AssetCache<Asset>>();
    auto [pair, is_loaded] = cache.load(id, path, reg);
    auto asset = pair->second;

    if (is_loaded) {
        TraceLog(LOG_INFO, "[ASSET] %s loaded successfully", path.c_str());
        auto& mgr = reg.ctx().get<AssetManager>();
        auto& vfs = reg.ctx().get<std::unique_ptr<vfs::Vfs>>();
        auto real_path = vfs->real_path(path);
        mgr.m_path_map.insert_or_assign(real_path, path);
    }

    return asset;
}

template<is_asset Asset>
inline auto AssetManager::register_asset(entt::registry& reg) -> void {
    if (!reg.ctx().contains<AssetManager>()) reg.ctx().emplace<AssetManager>();
    auto& mgr = reg.ctx().get<AssetManager>();

    reg.ctx().emplace<AssetCache<Asset>>();

    for (const auto& ext : Asset::extensions()) {
        mgr.m_handlers[ext].emplace_back([](const std::string& path, entt::registry& reg) -> void {
            auto& cache = reg.ctx().get<AssetCache<Asset>>();
            auto id = entt::hashed_string {path.c_str(), path.size()};
            if (cache.contains(id)) {
                auto asset = cache[id];
                try {
                    asset->reload(path, reg);
                    TraceLog(LOG_INFO, "[ASSET] %s reloaded successfully", path.c_str());
                } catch (const std::exception& e) {
                    TraceLog(LOG_ERROR, "[ASSET] ERROR: reloading %s: %s", path.c_str(), e.what());
                }
            }
        });
    }
}

} // namespace glint::core

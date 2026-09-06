#pragma once

#if GLINT_HOT_RELOAD_ASSETS

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <entt/entity/registry.hpp>
#include <fmt/format.h>
#include <raylib.h>
#include <wtr/watcher.hpp>

#include <glint/core/assets/asset.hpp>
#include <glint/core/assets/cache.hpp>
#include <glint/core/assets/concurrent_queue.hpp>
#include <glint/core/assets/resource.hpp>

namespace glint::core {

class AssetWatcher {
  public:
    AssetWatcher() = default;

    template<is_asset Asset>
    auto register_asset(entt::registry& registry) -> void;

    auto register_path(entt::registry& registry, const std::string& path) -> void;
    auto reload_pending_assets(entt::registry& registry) -> void;
    auto watch(std::string real_path, std::string virtual_path) -> void;

  private:
    using EventQueue = ConcurrentQueue<std::filesystem::path>;
    using EventHandler = std::function<void(const std::string&, entt::registry&)>;
    using Watch = std::shared_ptr<wtr::watch>;

    std::shared_ptr<EventQueue> m_event_queue = std::make_shared<EventQueue>();
    std::unordered_map<std::string, std::vector<EventHandler>> m_handlers {};
    std::unordered_map<std::string, std::string> m_path_map {};
    std::vector<Watch> m_watchers {};
};

} // namespace glint::core

namespace glint::core {

template<is_asset Asset>
inline auto AssetWatcher::register_asset(entt::registry& registry) -> void {
    for (const auto& ext : Asset::extensions()) {
        m_handlers[ext].emplace_back([](const std::string& path, entt::registry& registry) -> void {
            auto& cache = registry.ctx().get<AssetCache<Asset>>();
            if (cache.contains(path)) {
                auto asset = *cache.get(path);
                try {
                    asset->reload(path, registry);
                    TraceLog(LOG_INFO, "[ASSET] %s reloaded successfully", path.c_str());
                } catch (const std::exception& e) {
                    TraceLog(
                        LOG_ERROR,
                        "[ASSET] ERROR: exception occured while reloading %s: %s",
                        path.c_str(),
                        e.what()
                    );
                }
            }
        });
    }
}

} // namespace glint::core

#endif

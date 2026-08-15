#include <glint/core/asset_manager.hpp>

#include <fmt/format.h>

namespace glint::core {

auto AssetManager::register_path(entt::registry& reg, const std::string& path) -> void {
#if GLINT_HOT_RELOAD_ASSETS
    auto p = std::filesystem::canonical(path);
    auto pstr = p.string();

    auto& mgr = reg.ctx().get<AssetManager>();

    mgr.m_watchers.emplace_back(
        std::make_shared<wtr::watch>(path, [queue = mgr.m_event_queue, base = p](wtr::event event) -> void {
            auto effect = wtr::to<std::string>(event.effect_type);
            auto path_type = wtr::to<std::string>(event.path_type);
            auto path_name = wtr::to<std::string>(event.path_name);
            auto associated = (event.associated ? " -> " + wtr::to<std::string>(event.associated->path_name) : "");
            auto msg = fmt::format("[WATCH] EVENT: {} {} `{}`{}", effect, path_type, path_name, associated);
            TraceLog(LOG_DEBUG, "%s", msg.c_str());

            if (event.effect_type != wtr::event::effect_type::modify) return;

            msg = fmt::format("[WATCH] Reloading `{}`", path_name);
            TraceLog(LOG_INFO, "%s", msg.c_str());
            queue->push(std::move(event.path_name));
        })
    );
    TraceLog(LOG_INFO, "[WATCH] Registered watcher at `%s`", pstr.c_str());
#else
    (void)reg;
    (void)path;
#endif
}

auto AssetManager::update_assets(entt::registry& reg) -> void {
    auto& mgr = reg.ctx().get<AssetManager>();
    while (true) {
        auto path = mgr.m_event_queue->pop();
        if (!path) break;
        auto ext = path->extension().string();
        if (auto sea = mgr.m_handlers.find(ext); sea != mgr.m_handlers.end()) {
            const auto virtual_path = mgr.m_path_map[path->string()];
            for (auto& handler : sea->second) {
                handler(virtual_path, reg);
            }
        }
    }
}

auto update_assets(entt::registry& reg) -> void {
    AssetManager::update_assets(reg);
}

auto init_assets(entt::registry& reg) -> void {
    auto& vfs = get_vfs(reg);

    reg.ctx().emplace<AssetManager>();

    for (const auto& path : vfs.get_point_list()) {
        AssetManager::register_path(reg, vfs.real_path(path));
    }
}

} // namespace glint::core

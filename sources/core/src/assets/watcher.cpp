#include <glint/core/assets/watcher.hpp>

#if GLINT_HOT_RELOAD_ASSETS

namespace glint::core {

auto AssetWatcher::register_path(entt::registry& registry, const std::string& path) -> void {
    (void)registry;

    auto p = std::filesystem::canonical(path);
    auto pstr = p.string();

    m_watchers.emplace_back(
        std::make_shared<wtr::watch>(path, [queue = m_event_queue, base = p](wtr::event event) -> void {
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
}

auto AssetWatcher::reload_pending_assets(entt::registry& registry) -> void {
    while (true) {
        auto path = m_event_queue->pop();
        if (!path) break;
        auto ext = path->extension().string();
        if (auto handler_list = m_handlers.find(ext); handler_list != m_handlers.end()) {
            const auto virtual_path = m_path_map[path->string()];
            for (auto& handler : handler_list->second) {
                handler(virtual_path, registry);
            }
        }
    }
}

auto AssetWatcher::watch(std::string real_path, std::string virtual_path) -> void {
    m_path_map.insert_or_assign(std::move(real_path), std::move(virtual_path));
}

} // namespace glint::core

#endif

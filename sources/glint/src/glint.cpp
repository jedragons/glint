#include <glint/glint.hpp>

#include <exception>
#include <ranges>

#include <raylib.h>

#include <glint/core/assets.hpp>
#include <glint/core/lua.hpp>
#include <glint/core/vfs.hpp>

namespace glint {

static inline auto call_hooks(const GameHooks::HookList& hook_list, entt::registry& reg) -> void {
    for (auto& f : hook_list) {
        f(reg);
    }
}

static inline auto copy_hook(GameHooks::HookList& destination, const ModuleHook& hook) -> void {
    if (hook) {
        destination.emplace_back(hook);
    }
}

static inline auto copy_module_hooks(GameHooks& hooks, const Module& mod) -> void {
    copy_hook(hooks.load, mod.hooks.load);
    copy_hook(hooks.unload, mod.hooks.unload);
    copy_hook(hooks.update, mod.hooks.update);
    copy_hook(hooks.render, mod.hooks.render);
}

Game::Game(const Config& config) : m_window(config.window) {
    for (const auto& module : config.modules) {
        module.import(m_reg);
    }

    for (const auto& module : m_reg.ctx().emplace<ModuleContext>().modules | std::views::values) {
        copy_module_hooks(m_hooks, module);
    }

    call_hooks(m_hooks.load, m_reg);

    auto& vfs = core::get_vfs(m_reg);

    auto mount_points = config.mount_points;
    if (mount_points.empty()) {
        mount_points.emplace_back(MountConfig {.point = "/", .root = GetWorkingDirectory(), .kind = MountKind::Native});
    }

    for (const auto& mount : mount_points) {
        TraceLog(LOG_INFO, "Mounting %s to %s", mount.point.c_str(), mount.root.c_str());
        vfs.mount(mount.point, mount.root);
    }

    if (!config.initial_module.empty()) {
        auto initial = m_reg.create();
        auto module = core::load_asset<core::LuaAsset>(m_reg, config.initial_module);
        m_reg.emplace<core::Behavior>(initial, module);
    }
}

auto Game::registry() -> entt::registry& {
    return m_reg;
}

auto Game::registry() const -> const entt::registry& {
    return m_reg;
}

auto Game::run() -> int try {
    while (!WindowShouldClose()) {
        frame();
    }
    return 0;
} catch (const std::exception& e) {
    TraceLog(LOG_ERROR, "Exception occured while running game: %s", e.what());
    return 1;
}

auto Game::is_running() -> bool {
    return !WindowShouldClose();
}

auto Game::frame() -> void {
    call_hooks(m_hooks.update, m_reg);
    BeginDrawing();
    ClearBackground(BLACK);
    call_hooks(m_hooks.render, m_reg);
    EndDrawing();
}

} // namespace glint

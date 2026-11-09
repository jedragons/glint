#include <glint/glint.hpp>

#include <ranges>

#include <gsl/gsl>

#include <glint/core/core.hpp>
#include <glint/platform.hpp>

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

Game::Game(const Config& config) :
    m_window(Window::create(config.window)),
    m_renderer(Renderer::create()),
    m_audio_device(AudioDevice::create()) {
    core::set_window(m_reg, m_window);
    core::set_renderer(m_reg, m_renderer);

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
        mount_points.emplace_back(
            MountConfig {.point = "/", .root = platform::get_working_directory(), .kind = MountKind::Native}
        );
    }

    for (const auto& mount : mount_points) {
        log_info("Mounting {} to {}", mount.point.c_str(), mount.root.c_str());
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

auto Game::init() -> void {}

auto Game::frame() -> void {
    call_hooks(m_hooks.update, m_reg);
    core::get_renderer(m_reg).clear_background(Black);
    call_hooks(m_hooks.render, m_reg);
}

auto Game::close() -> void {
    platform::log_info("Closing game");
    glint::call_hooks(m_hooks.unload, m_reg);
}

} // namespace glint

extern "C" {

auto glint_init(void** app, int argc, char** argv) -> GlintAppResult {
    (void)argc;
    (void)argv;

    auto config = glint_main();
    auto game = gsl::owner<glint::Game*>(new glint::Game {config});
    game->init();
    *app = static_cast<void*>(game);
    return GLINT_APP_CONTINUE;
}

auto glint_frame(void* app) -> GlintAppResult {
    auto& game = *static_cast<glint::Game*>(app);
    game.frame();
    return GLINT_APP_CONTINUE;
}

auto glint_close(void* app) -> GlintAppResult {
    auto game = gsl::owner<glint::Game*>(app);
    game->close();
    delete game;
    return GLINT_APP_CLOSE;
}
}

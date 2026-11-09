#pragma once

#include <vector>

#include <entt/entity/registry.hpp>

#include <glint/config.hpp>
#include <glint/core/core.hpp>
#include <glint/module.hpp>
#include <glint/platform.hpp>

namespace glint {

struct GameHooks {
    using HookList = std::vector<ModuleHook>;

    HookList load;
    HookList unload;
    HookList update;
    HookList render;
};

class Game final {
  public:
    explicit Game(const Config& config = default_config());

    [[nodiscard]]
    auto registry() -> entt::registry&;

    [[nodiscard]]
    auto registry() const -> const entt::registry&;

  private:
    entt::registry m_reg {};
    GameHooks m_hooks {};
    Window m_window;
    Renderer m_renderer;
    AudioDevice m_audio_device;

    auto init() -> void;
    auto frame() -> void;
    auto close() -> void;

    friend auto ::glint_init(void** app, int argc, char** argv) -> GlintAppResult;
    friend auto ::glint_frame(void* app) -> GlintAppResult;
    friend auto ::glint_close(void* app) -> GlintAppResult;
};

} // namespace glint

auto glint_main() -> glint::Config;

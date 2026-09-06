#pragma once

#include <vector>

#include <entt/entity/registry.hpp>

#include <glint/config.hpp>
#include <glint/core/core.hpp>
#include <glint/module.hpp>

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
    Game(const Config& config = default_config());

    [[nodiscard]]
    auto registry() -> entt::registry&;

    [[nodiscard]]
    auto registry() const -> const entt::registry&;

    auto run() -> int;

    auto is_running() -> bool;

    auto frame() -> void;

  private:
    entt::registry m_reg {};
    GameHooks m_hooks {};
    core::Window m_window {};
};

} // namespace glint

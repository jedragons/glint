#pragma once

#include <entt/entt.hpp>
#include <raylib-cpp/raylib-cpp.hpp>
#include <sol/sol.hpp>

#include <glint/config_builder.hpp>
#include <glint/core.hpp>

namespace glint {

using Callback = std::function<void(entt::registry&)>;
using CallbackList = std::vector<Callback>;

class Game final {
  public:
    Game(const core::Config& config = default_config());
    Game(const Game&) = delete;
    Game(Game&&) = delete;
    auto operator=(const Game&) -> Game& = delete;
    auto operator=(Game&&) -> Game& = delete;
    ~Game();

    [[nodiscard]]
    auto registry() -> entt::registry&;

    [[nodiscard]]
    auto registry() const -> const entt::registry&;

    auto run() -> int;

    auto is_running() -> bool;

    auto frame() -> void;

  private:
    entt::registry m_reg {};

    CallbackList m_pre_init {};
    CallbackList m_init {};
    CallbackList m_post_init {};
    CallbackList m_pre_deinit {};
    CallbackList m_deinit {};
    CallbackList m_post_deinit {};
    CallbackList m_pre_update {};
    CallbackList m_update {};
    CallbackList m_post_update {};
    CallbackList m_pre_render {};
    CallbackList m_render {};
    CallbackList m_post_render {};

    auto call(CallbackList& list) -> void;
};

} // namespace glint

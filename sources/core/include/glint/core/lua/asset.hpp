#pragma once

#include <string>
#include <vector>

#include <entt/fwd.hpp>
#include <sol/sol.hpp>

#include <glint/core/assets.hpp>

namespace glint::core {

class LuaAsset {
  public:
    LuaAsset(std::string path, entt::registry& registry);

    static auto extensions() -> std::vector<std::string>;
    static auto load(const std::string& path, entt::registry& registry) -> LuaAsset;

    auto reload(this LuaAsset& self, const std::string& path, entt::registry&) -> void;
    auto setup(this LuaAsset& self, entt::handle entity) -> void;
    auto destroy(this LuaAsset& self, entt::handle entity) -> void;
    auto update(this LuaAsset& self, entt::handle entity) -> void;

  private:
    std::shared_ptr<sol::state> state {};
    std::string path {};
    sol::environment env {};
    sol::protected_function m_setup_func {};
    sol::protected_function m_destroy_func {};
    sol::protected_function m_update_func {};
    bool error_state {false};
};

static_assert(is_asset<LuaAsset>);

} // namespace glint::core

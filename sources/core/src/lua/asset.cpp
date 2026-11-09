#include <glint/core/lua/asset.hpp>

#include <expected>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/table.hpp>

#include <glint/core/assets.hpp>
#include <glint/core/lua.hpp>
#include <glint/core/vfs.hpp>

namespace glint::core {

static auto load_script(entt::registry& registry, const std::string& path) -> std::expected<sol::table, sol::error> {
    auto& vfs = get_vfs(registry);
    auto lua = get_lua(registry);
    auto code = vfs.read_str(path);
    auto result = lua.load(code);

    if (!result.valid()) {
        auto error = sol::error(result);
        log_error("[LUA] {}: could not load script: {}", path, error.what());
        return std::unexpected(error);
    }

    return result.call<sol::table>();
}

LuaAsset::LuaAsset(std::string path, entt::registry& registry) :
    state(registry.ctx().get<std::shared_ptr<sol::state>>()),
    path(path) {
    auto result = load_script(registry, path);

    if (!result) {
        error_state = true;
        return;
    }

    m_setup_func = result->get<sol::protected_function>("setup");
    m_destroy_func = result->get<sol::protected_function>("destroy");
    m_update_func = result->get<sol::protected_function>("update");
}

auto LuaAsset::extensions() -> std::vector<std::string> {
    return {".lua"};
}

auto LuaAsset::load(const std::string& path, entt::registry& registry) -> LuaAsset {
    return {path, registry};
}

auto LuaAsset::setup(this LuaAsset& self, entt::handle entity) -> void {
    if (self.error_state || !self.m_setup_func.valid()) return;

    auto result = self.m_setup_func.call(entity);

    if (!result.valid()) {
        auto err = sol::error(result);
        log_error("[LUA] {}: {}", self.path, err.what());
        self.error_state = true;
        return;
    }
}

auto LuaAsset::destroy(this LuaAsset& self, entt::handle entity) -> void {
    if (self.error_state || !self.m_destroy_func.valid()) return;

    auto result = self.m_destroy_func.call(entity);

    if (!result.valid()) {
        auto err = sol::error(result);
        log_error("[LUA] {}: {}", self.path, err.what());
        self.error_state = true;
        return;
    }
}

auto LuaAsset::update(this LuaAsset& self, entt::handle entity) -> void {
    if (self.error_state || !self.m_update_func.valid()) return;

    auto result = self.m_update_func.call(entity);

    if (!result.valid()) {
        auto err = sol::error(result);
        log_error("[LUA] {}: {}", self.path, err.what());
        self.error_state = true;
        return;
    }
}

auto LuaAsset::reload(this LuaAsset& self, const std::string&, entt::registry& registry) -> void {
    auto result = load_script(registry, self.path);
    if (!result) return;

    self.error_state = false;
    self.m_setup_func = result->get<sol::protected_function>("setup");
    self.m_destroy_func = result->get<sol::protected_function>("destroy");
    self.m_update_func = result->get<sol::protected_function>("update");
}

} // namespace glint::core

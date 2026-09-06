#include <glint/core/lua/asset.hpp>

#include <expected>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>

#include <glint/core/assets.hpp>
#include <glint/core/lua.hpp>
#include <glint/core/vfs.hpp>

namespace glint::core {

static auto load_script(entt::registry& registry, const std::string& path, sol::environment& env)
    -> std::expected<std::monostate, sol::error> {
    auto& vfs = get_vfs(registry);
    auto lua = get_lua(registry);
    auto code = vfs.read_str(path);
    auto result = lua.safe_script(code, env, sol::script_pass_on_error);

    if (result.valid()) {
        TraceLog(LOG_INFO, "[LUA] %s: script loaded successfully", path.c_str());
        return std::monostate {};
    } else {
        auto error = sol::error(result);
        TraceLog(LOG_ERROR, "[LUA] %s: could not load script: %s", path.c_str(), error.what());
        return std::unexpected(error);
    }
}

LuaAsset::LuaAsset(std::string path, entt::registry& registry) :
    state(registry.ctx().get<std::shared_ptr<sol::state>>()),
    path(path),
    env(*state, sol::create, state->globals()) {
    auto result = load_script(registry, path, env);
    if (!result) {
        error_state = true;
        return;
    }

    m_setup_func = env["Setup"];
    m_destroy_func = env["Destroy"];
    m_update_func = env["Update"];
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
        TraceLog(LOG_ERROR, "[LUA] %s: %s", self.path.c_str(), err.what());
        self.error_state = true;
        return;
    }
}

auto LuaAsset::destroy(this LuaAsset& self, entt::handle entity) -> void {
    if (self.error_state || !self.m_destroy_func.valid()) return;

    auto result = self.m_destroy_func.call(entity);

    if (!result.valid()) {
        auto err = sol::error(result);
        TraceLog(LOG_ERROR, "[LUA] %s: %s", self.path.c_str(), err.what());
        self.error_state = true;
        return;
    }
}

auto LuaAsset::update(this LuaAsset& self, entt::handle entity) -> void {
    if (self.error_state || !self.m_update_func.valid()) return;

    auto result = self.m_update_func.call(entity);

    if (!result.valid()) {
        auto err = sol::error(result);
        TraceLog(LOG_ERROR, "[LUA] %s: %s", self.path.c_str(), err.what());
        self.error_state = true;
        return;
    }
}

auto LuaAsset::reload(this LuaAsset& self, const std::string&, entt::registry& registry) -> void {
    auto new_env = sol::environment(*self.state, sol::create, self.state->globals());

    auto result = load_script(registry, self.path, new_env);
    if (!result) return;

    self.error_state = false;
    self.env = new_env;
    self.m_setup_func = self.env["Setup"];
    self.m_destroy_func = self.env["Destroy"];
    self.m_update_func = self.env["Update"];
}

} // namespace glint::core

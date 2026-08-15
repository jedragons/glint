#include <glint/core/lua.hpp>

#include <glint/core/asset/lua_script.hpp>
#include <glint/core/asset_manager.hpp>
#include <glint/core/ecs/lua_module.hpp>

namespace glint::core {

auto init_lua(entt::registry& reg) -> void {
    auto& lua = reg.ctx().emplace<std::shared_ptr<sol::state>>(std::make_shared<sol::state>());
    lua->open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table, sol::lib::debug);
    AssetManager::register_asset<LuaAsset>(reg);
}

auto update_lua(entt::registry& reg) -> void {
    for (auto [entity, comp] : reg.view<LuaPendingComp>().each()) {
        auto module = comp.module;
        reg.erase<LuaPendingComp>(entity);
        reg.emplace<LuaComp>(entity, module);
    }

    for (auto [entity, comp] : reg.view<LuaComp>().each()) {
        comp.module->update(entt::handle(reg, entity));
    }
}

auto get_lua(entt::registry& reg) -> sol::state& {
    return *reg.ctx().get<std::shared_ptr<sol::state>>();
}

} // namespace glint::core

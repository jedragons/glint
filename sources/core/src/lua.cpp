#include <glint/core/lua.hpp>

#include <memory>

#include <sol/sol.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>

#include <glint/core/assets.hpp>
#include <glint/core/hierarchy.hpp>

namespace glint::core {

static inline auto setup_behavior(entt::registry& registry, entt::entity entity) -> void {
    auto& behavior = registry.get<Behavior>(entity);
    behavior.module->setup(entt::handle {registry, entity});
}

static inline auto teardown_behavior(entt::registry& registry, entt::entity entity) -> void {
    auto& behavior = registry.get<Behavior>(entity);
    behavior.module->destroy(entt::handle {registry, entity});
}

static inline auto import_lua(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    assets_module().import(registry);
    hierarchy_module().import(registry);
    register_asset<LuaAsset>(registry);

    auto lua = std::make_shared<sol::state>();
    lua->open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table, sol::lib::debug);
    registry.ctx().emplace<std::shared_ptr<sol::state>>(lua);

    registry.on_construct<Behavior>().connect<&setup_behavior>();
    registry.on_destroy<Behavior>().connect<&teardown_behavior>();

    bind_entity(registry);
    bind_registry(registry);
}

static inline auto update_lua(entt::registry& registry) -> void {
    for (auto [entity, behavior] : registry.view<Behavior>().each()) {
        behavior.module->update(entt::handle {registry, entity});
    }
}

auto lua_module() -> Module {
    return Module {
        .name = "glint.core.lua",
        .hooks = {.import = import_lua, .update = update_lua},
    };
}

auto get_lua(entt::registry& registry) -> sol::state_view {
    return {*registry.ctx().get<std::shared_ptr<sol::state>>()};
}

} // namespace glint::core

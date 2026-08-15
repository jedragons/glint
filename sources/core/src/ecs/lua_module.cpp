#include <glint/core/ecs/lua_module.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>

namespace glint::core {

void LuaComp::on_construct(entt::registry& registry, const entt::entity e) {
    auto comp = registry.get<LuaComp>(e);
    auto handle = entt::handle {registry, e};
    handle.emplace<LuaDataComp>();
    comp.module->setup(handle);
}

void LuaComp::on_destroy(entt::registry& registry, const entt::entity e) {
    auto comp = registry.get<LuaComp>(e);
    auto handle = entt::handle {registry, e};
    comp.module->destroy(handle);
}

} // namespace glint::core

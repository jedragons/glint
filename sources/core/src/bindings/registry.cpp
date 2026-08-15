#include <glint/core/bindings/registry.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/ecs/lua_module.hpp>
#include <glint/core/ecs/name_map.hpp>
#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_registry(entt::registry& reg) -> void {
    auto& lua = core::get_lua(reg);

    /*@
      ---@overload fun(): Entity
      ---@param script_path string
      ---@return Entity
      function spawn(script_path) end
    */
    lua.globals()["spawn"] = sol::overload(
        [&reg]() -> entt::handle { return {reg, reg.create()}; },
        [&reg](const std::string& path) -> entt::handle {
            auto module = load_module(reg, path);
            auto handle = entt::handle {reg, reg.create()};
            handle.emplace<LuaPendingComp>(module);
            return handle;
        }
    );
}

} // namespace glint::core

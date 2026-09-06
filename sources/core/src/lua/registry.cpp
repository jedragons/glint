#include <glint/core/lua/registry.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/assets.hpp>
#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_registry(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    /*@
      ---@overload fun(): Entity
      ---@param script_path string
      ---@return Entity
      function spawn(script_path) end
     */
    lua.globals()["spawn"] = sol::overload(
        [&registry]() -> entt::handle { return {registry, registry.create()}; },
        [&registry](const std::string& path) -> entt::handle {
            auto module = load_asset<LuaAsset>(registry, path);
            auto handle = entt::handle {registry, registry.create()};
            handle.emplace<Behavior>(module);
            return handle;
        }
    );
}

} // namespace glint::core

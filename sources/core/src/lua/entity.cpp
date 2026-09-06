#include <glint/core/lua/entity.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <fmt/format.h>
#include <sol/sol.hpp>

#include <glint/core/assets.hpp>
#include <glint/core/hierarchy/name_map.hpp>
#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_entity(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<entt::handle>(
        /*@
          ---@class Entity
          ---@field name string
          Entity = {}
         */
        "Entity",

        sol::meta_function::index,
        [](entt::handle& handle, const std::string& key) -> sol::object {
            auto& behavior = handle.get<Behavior>();
            auto it = behavior.data.find(key);
            if (it != behavior.data.end()) return it->second;
            return sol::lua_nil;
        },

        sol::meta_function::new_index,
        [](entt::handle& handle, const std::string& key, sol::object value) -> void {
            auto& behavior = handle.get<Behavior>();
            behavior.data.insert_or_assign(key, value);
        },

        "name",
        sol::property(
            [](entt::handle& handle) -> std::optional<std::string> {
                if (!handle.all_of<NameComp>()) return std::nullopt;
                return handle.get<NameComp>().name();
            },
            [](entt::handle& handle, std::optional<std::string> name) -> void {
                if (name) {
                    handle.emplace_or_replace<NameComp>(NameComp {std::move(*name)});
                } else {
                    handle.erase<NameComp>();
                }
            }
        ),

        /*@
          ---@param name string
          ---@return Entity
          function Entity.find(name) end
         */
        "find",
        [&registry](const std::string& name) -> entt::handle {
            const auto e = registry.ctx().get<NameMap>().get(name);
            if (e == entt::null) {
                throw sol::error(fmt::format("Entity with name `{}` does not exist", name));
            }
            return {registry, e};
        },

        /*@
          ---@param name string
          ---@return Entity|nil
          function Entity.try_find(name) end
         */
        "try_find",
        [&registry](const std::string& name) -> std::optional<entt::handle> {
            const auto e = registry.ctx().get<NameMap>().get(name);
            if (e == entt::null) return std::nullopt;
            return entt::handle {registry, e};
        },

        /*@
          ---@return boolean
          function Entity:valid() end
         */
        "valid",
        [](entt::handle& handle) -> bool { return handle.valid(); },

        /*@
          function Entity:destroy() end
         */
        "destroy",
        [](entt::handle& handle) -> void { handle.destroy(); },

        /*@
          ---@param path string
          function Entity:attach_module(path) end
         */
        "attach_module",
        [](entt::handle& handle, const std::string& path) -> void {
            auto module = load_asset<LuaAsset>(*handle.registry(), path);
            handle.emplace_or_replace<Behavior>(module);
        }
    );
}

} // namespace glint::core

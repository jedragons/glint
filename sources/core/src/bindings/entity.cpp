#include <glint/core/bindings/entity.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <fmt/format.h>
#include <sol/sol.hpp>

#include <glint/core/ecs/lua_module.hpp>
#include <glint/core/ecs/name_map.hpp>
#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_entity(entt::registry& reg) -> void {
    auto& lua = core::get_lua(reg);

    lua.new_usertype<entt::handle>(
        /*@
          ---@class Entity
          ---@field name string
          Entity = {}
        */
        "Entity",

        sol::meta_function::index,
        [](entt::handle& handle, const std::string& key) -> sol::object {
            auto& comp = handle.get<LuaDataComp>();
            auto it = comp.data.find(key);
            if (it != comp.data.end()) return it->second;
            return sol::lua_nil;
        },

        sol::meta_function::new_index,
        [](entt::handle& handle, const std::string& key, sol::object data) -> void {
            auto& comp = handle.get<LuaDataComp>();
            comp.data.insert_or_assign(key, data);
        },

        "name",
        sol::property(
            [](entt::handle& handle) -> std::optional<std::string> {
                if (handle.all_of<NameComp>()) return std::nullopt;
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
        [&reg](const std::string& name) -> entt::handle {
            const auto e = reg.ctx().get<NameMap>().get(name);
            if (e == entt::null) {
                throw sol::error(fmt::format("Entity with name `{}` does not exist", name));
            }
            return {reg, e};
        },

        /*@
          ---@param name string
          ---@return Entity|nil
          function Entity.try_find(name) end
        */
        "try_find",
        [&reg](const std::string& name) -> std::optional<entt::handle> {
            const auto e = reg.ctx().get<NameMap>().get(name);
            if (e == entt::null) return std::nullopt;
            return entt::handle {reg, e};
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
            auto module = load_module(*handle.registry(), path);
            handle.emplace_or_replace<LuaPendingComp>(module);
        }
    );
}

} // namespace glint::core

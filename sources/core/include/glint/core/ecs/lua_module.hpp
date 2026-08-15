#pragma once

#include <entt/resource/resource.hpp>

#include <glint/core/asset/lua_script.hpp>

namespace glint::core {

struct LuaComp {
    entt::resource<LuaAsset> module;

    static auto on_construct(entt::registry& registry, const entt::entity e) -> void;
    static auto on_destroy(entt::registry& registry, const entt::entity e) -> void;
};

struct LuaPendingComp {
    entt::resource<LuaAsset> module;
};

struct LuaDataComp {
    std::unordered_map<std::string, sol::object> data {};
};

} // namespace glint::core

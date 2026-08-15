#pragma once

#include <entt/fwd.hpp>
#include <sol/forward.hpp>

#include <glint/core/asset.hpp>

#define GLINT_COMPONENT_FUNC_GET(type) \
    ("get"), ([](entt::handle& h) -> type& { \
        if (!h.all_of<type>()) { \
            throw sol::error("Entity does not have component of type `" #type "`"); \
        } \
        return h.get<type>(); \
    })

#define GLINT_COMPONENT_FUNC_HAS(type) "has", ([](entt::handle& h) -> bool { return h.all_of<type>(); })

#define GLINT_COMPONENT_FUNC_REMOVE(type) "remove", [](entt::handle& h) -> void { h.remove<type>(); }

#define GLINT_COMPONENT_FUNCS(type) \
    GLINT_COMPONENT_FUNC_GET(type), GLINT_COMPONENT_FUNC_HAS(type), GLINT_COMPONENT_FUNC_REMOVE(type)

namespace glint::core {

auto init_lua(entt::registry& reg) -> void;
auto update_lua(entt::registry& reg) -> void;
auto get_lua(entt::registry& reg) -> sol::state&;

} // namespace glint::core

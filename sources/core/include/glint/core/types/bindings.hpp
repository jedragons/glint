#pragma once

#include <entt/fwd.hpp>

namespace glint::core {

auto bind_vector2(entt::registry& registry) -> void;
auto bind_rectangle(entt::registry& registry) -> void;
auto bind_color(entt::registry& registry) -> void;

}

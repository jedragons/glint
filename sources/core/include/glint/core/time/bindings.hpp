#pragma once

#include <entt/fwd.hpp>

namespace glint::core {

auto bind_stopwatch(entt::registry& registry) -> void;
auto bind_timer(entt::registry& registry) -> void;

} // namespace glint::core

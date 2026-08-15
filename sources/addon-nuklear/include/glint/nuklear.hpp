#pragma once

#include <entt/fwd.hpp>
#include <nuklear.h>

namespace glint::nuklear {

auto get_nuklear(entt::registry& reg) -> nk_context*;

} // namespace glint::nuklear

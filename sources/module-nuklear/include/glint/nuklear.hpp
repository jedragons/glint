#pragma once

#include <entt/fwd.hpp>

#include <glint/module.hpp>

struct nk_context;

namespace glint::nuklear {

auto get_nuklear(entt::registry& registry) -> nk_context*;

} // namespace glint::nuklear

namespace glint {

auto nuklear_module() -> Module;

} // namespace glint

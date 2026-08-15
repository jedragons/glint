#include <glint/nuklear.hpp>

#include <entt/entity/registry.hpp>

namespace glint::nuklear {

auto get_nuklear(entt::registry& reg) -> nk_context* {
    return reg.ctx().get<nk_context*>();
}

} // namespace glint::nuklear

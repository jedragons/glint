#pragma once

#include <entt/fwd.hpp>

#include <glint/vfs/vfs.hpp>

namespace glint::core {

auto init_vfs(entt::registry& reg) -> void;
auto deinit_vfs(entt::registry& reg) -> void;
auto get_vfs(entt::registry& reg) -> vfs::Vfs&;

} // namespace glint::core

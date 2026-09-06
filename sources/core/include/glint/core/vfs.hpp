#pragma once

#include <glint/module.hpp>
#include <glint/vfs/vfs.hpp>

namespace glint::core {

auto vfs_module() -> Module;

auto get_vfs(entt::registry& registry) -> vfs::Vfs&;

} // namespace glint::core

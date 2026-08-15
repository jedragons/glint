#pragma once

#include <entt/entt.hpp>

#include <glint/core/asset.hpp>
#include <glint/core/asset/lua_script.hpp>
#include <glint/core/asset/texture.hpp>
#include <glint/core/asset_manager.hpp>
#include <glint/core/config.hpp>
#include <glint/core/ecs/lua_module.hpp>
#include <glint/core/ecs/name_map.hpp>
#include <glint/core/ecs/stopwatch.hpp>
#include <glint/core/ecs/timer.hpp>
#include <glint/core/lua.hpp>
#include <glint/core/vfs.hpp>
#include <glint/core/window.hpp>

namespace glint {

auto init_core(entt::registry& reg, const core::Config& config) -> void;
auto deinit_core(entt::registry& reg) -> void;
auto update_core(entt::registry& reg) -> void;

} // namespace glint

#pragma once

#include <entt/fwd.hpp>

#include <raylib-cpp/Window.hpp>
#include <sol/forward.hpp>

#include <glint/core/config.hpp>

namespace glint::core {

auto init_window(entt::registry& reg) -> void;
auto deinit_window(entt::registry& reg) -> void;
auto get_window(entt::registry& reg) -> raylib::Window&;

auto register_window_lua(entt::registry& reg) -> void;

} // namespace glint::core

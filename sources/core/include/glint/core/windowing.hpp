#pragma once

#include <glint/core/windowing/bindings.hpp>
#include <glint/module.hpp>
#include <glint/platform/window.hpp>

namespace glint::core {

auto windowing_module() -> Module;

auto set_window(entt::registry& reg, Window& window) -> void;

auto get_window(entt::registry& reg) -> Window&;

} // namespace glint::core

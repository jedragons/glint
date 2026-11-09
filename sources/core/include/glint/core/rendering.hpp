#pragma once

#include <glint/module.hpp>
#include <glint/platform/renderer.hpp>

namespace glint::core {

auto rendering_module() -> Module;

auto set_renderer(entt::registry& reg, Renderer& renderer) -> void;

auto get_renderer(entt::registry& reg) -> Renderer&;

} // namespace glint::core

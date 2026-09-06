#pragma once

#include <glint/dim2/rendering/asset/atlas.hpp>
#include <glint/dim2/rendering/bindings/animated_sprite.hpp>
#include <glint/dim2/rendering/bindings/atlas.hpp>
#include <glint/dim2/rendering/bindings/sprite.hpp>
#include <glint/dim2/rendering/ecs/animated_sprite.hpp>
#include <glint/dim2/rendering/ecs/atlas.hpp>
#include <glint/dim2/rendering/ecs/sprite.hpp>
#include <glint/module.hpp>

namespace glint::dim2 {

auto rendering_module() -> Module;

} // namespace glint::dim2

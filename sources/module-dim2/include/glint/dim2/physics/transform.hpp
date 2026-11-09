#pragma once

#include <glint/math.hpp>

namespace glint::dim2 {

struct Transform2D {
    Vec2 position = {};
    Vec2 scale = {.x = 1.0f, .y = 1.0f};
    Vec2 origin = {};
    float rotation = {};
};

} // namespace glint::dim2

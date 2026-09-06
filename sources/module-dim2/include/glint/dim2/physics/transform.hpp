#pragma once

#include <raylib.h>

namespace glint::dim2 {

struct Transform2D {
    Vector2 position {};
    Vector2 scale {1.0f, 1.0f};
    Vector2 origin {};
    float rotation {};
};

} // namespace glint::dim2

#pragma once

#include <raylib-cpp/Vector2.hpp>

namespace glint::dim2 {

struct Transform2D {
    raylib::Vector2 position {};
    raylib::Vector2 scale {1.0f, 1.0f};
    raylib::Vector2 origin {};
    float rotation {};
};

} // namespace glint::dim2

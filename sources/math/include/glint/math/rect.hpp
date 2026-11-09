#pragma once

namespace glint::math {

template<typename T = float>
struct Rectangle {
    T x;
    T y;
    T w;
    T h;
};

} // namespace glint::math

namespace glint {

using Rect = math::Rectangle<>;
using math::Rectangle;

} // namespace glint

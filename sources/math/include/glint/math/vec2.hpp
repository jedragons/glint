#pragma once

#include <cmath>
#include <optional>

namespace glint::math {

template<typename T = float>
struct Vector2 {
    T x;
    T y;

    auto operator+(this Vector2 self, Vector2 other) -> Vector2 {
        return {.x = self.x + other.x, .y = self.y + other.y};
    }

    auto operator-(this Vector2 self, Vector2 other) -> Vector2 {
        return {.x = self.x - other.x, .y = self.y - other.y};
    }

    auto operator*(this Vector2 self, Vector2 other) -> Vector2 {
        return {.x = self.x * other.x, .y = self.y * other.y};
    }

    auto operator/(this Vector2 self, Vector2 other) -> Vector2 {
        return {.x = self.x / other.x, .y = self.y / other.y};
    }

    auto length(this Vector2 self) -> T {
        return std::hypot(self.x, self.y);
    }

    auto normalize(this Vector2 self) -> std::optional<Vector2> {
        auto len = self.length();
        if (len == 0) return std::nullopt;

        return Vector2 {.x = self.x / len, .y = self.y / len};
    }

    auto scale(this Vector2 self, T scale) -> Vector2 {
        return {.x = self.x * scale, .y = self.y * scale};
    }
};

} // namespace glint::math

namespace glint {

using math::Vector2;
using Vec2 = math::Vector2<>;
using Vec2i = math::Vector2<int32_t>;
using Vec2d = math::Vector2<double>;

} // namespace glint

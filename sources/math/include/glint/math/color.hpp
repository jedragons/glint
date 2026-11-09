#pragma once

#include <cstdint>

namespace glint::math {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    static auto from_hex(uint32_t hex) -> Color {
        return {
            .r = static_cast<uint8_t>((hex >> 24u) & 0xffu),
            .g = static_cast<uint8_t>((hex >> 16u) & 0xffu),
            .b = static_cast<uint8_t>((hex >> 8u) & 0xffu),
            .a = static_cast<uint8_t>((hex >> 0u) & 0xffu),
        };
    }
};

} // namespace glint::math

namespace glint {

using math::Color;

constexpr auto Beige = Color {.r = 211, .g = 176, .b = 131, .a = 255};
constexpr auto Black = Color {.r = 0, .g = 0, .b = 0, .a = 255};
constexpr auto Blank = Color {.r = 0, .g = 0, .b = 0, .a = 0};
constexpr auto Blue = Color {.r = 0, .g = 121, .b = 241, .a = 255};
constexpr auto Brown = Color {.r = 127, .g = 106, .b = 79, .a = 255};
constexpr auto DarkBlue = Color {.r = 0, .g = 82, .b = 172, .a = 255};
constexpr auto DarkBrown = Color {.r = 76, .g = 63, .b = 47, .a = 255};
constexpr auto DarkGray = Color {.r = 80, .g = 80, .b = 80, .a = 255};
constexpr auto DarkGreen = Color {.r = 0, .g = 117, .b = 44, .a = 255};
constexpr auto DarkPurple = Color {.r = 112, .g = 31, .b = 126, .a = 255};
constexpr auto Gold = Color {.r = 255, .g = 203, .b = 0, .a = 255};
constexpr auto Gray = Color {.r = 130, .g = 130, .b = 130, .a = 255};
constexpr auto Green = Color {.r = 0, .g = 228, .b = 48, .a = 255};
constexpr auto LightGray = Color {.r = 200, .g = 200, .b = 200, .a = 255};
constexpr auto Lime = Color {.r = 0, .g = 158, .b = 47, .a = 255};
constexpr auto Magenta = Color {.r = 255, .g = 0, .b = 255, .a = 255};
constexpr auto Maroon = Color {.r = 190, .g = 33, .b = 55, .a = 255};
constexpr auto Orange = Color {.r = 255, .g = 161, .b = 0, .a = 255};
constexpr auto Pink = Color {.r = 255, .g = 109, .b = 194, .a = 255};
constexpr auto Purple = Color {.r = 200, .g = 122, .b = 255, .a = 255};
constexpr auto Red = Color {.r = 230, .g = 41, .b = 55, .a = 255};
constexpr auto SkyBlue = Color {.r = 102, .g = 191, .b = 255, .a = 255};
constexpr auto Violet = Color {.r = 135, .g = 60, .b = 190, .a = 255};
constexpr auto White = Color {.r = 255, .g = 255, .b = 255, .a = 255};
constexpr auto Yellow = Color {.r = 253, .g = 249, .b = 0, .a = 255};

} // namespace glint

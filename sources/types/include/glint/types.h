#ifndef GTYPES_TYPES_H
#define GTYPES_TYPES_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 2D value types mirroring the old glint::math definitions. The Glint prefix
 * keeps them distinct from raylib's own Vector2, Rectangle and Color, and this
 * header deliberately includes no raylib or Lua headers. */

typedef struct gvec2_Vec2 {
    float x;
    float y;
} gvec2_Vec2;

typedef struct grect_Rect {
    float x;
    float y;
    float w;
    float h;
} grect_Rect;

typedef struct gcolor_Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} gcolor_Color;

/* 0xRRGGBBAA. */
static inline gcolor_Color gcolor_from_hex(uint32_t hex) {
    return (gcolor_Color) {
        .r = (uint8_t)((hex >> 24u) & 0xffu),
        .g = (uint8_t)((hex >> 16u) & 0xffu),
        .b = (uint8_t)((hex >> 8u) & 0xffu),
        .a = (uint8_t)(hex & 0xffu),
    };
}

#define GCOLOR_BEIGE ((gcolor_Color){.r = 211, .g = 176, .b = 131, .a = 255})
#define GCOLOR_BLACK ((gcolor_Color){.r = 0, .g = 0, .b = 0, .a = 255})
#define GCOLOR_BLANK ((gcolor_Color){.r = 0, .g = 0, .b = 0, .a = 0})
#define GCOLOR_BLUE ((gcolor_Color){.r = 0, .g = 121, .b = 241, .a = 255})
#define GCOLOR_BROWN ((gcolor_Color){.r = 127, .g = 106, .b = 79, .a = 255})
#define GCOLOR_DARKBLUE ((gcolor_Color){.r = 0, .g = 82, .b = 172, .a = 255})
#define GCOLOR_DARKBROWN ((gcolor_Color){.r = 76, .g = 63, .b = 47, .a = 255})
#define GCOLOR_DARKGRAY ((gcolor_Color){.r = 80, .g = 80, .b = 80, .a = 255})
#define GCOLOR_DARKGREEN ((gcolor_Color){.r = 0, .g = 117, .b = 44, .a = 255})
#define GCOLOR_DARKPURPLE ((gcolor_Color){.r = 112, .g = 31, .b = 126, .a = 255})
#define GCOLOR_GOLD ((gcolor_Color){.r = 255, .g = 203, .b = 0, .a = 255})
#define GCOLOR_GRAY ((gcolor_Color){.r = 130, .g = 130, .b = 130, .a = 255})
#define GCOLOR_GREEN ((gcolor_Color){.r = 0, .g = 228, .b = 48, .a = 255})
#define GCOLOR_LIGHTGRAY ((gcolor_Color){.r = 200, .g = 200, .b = 200, .a = 255})
#define GCOLOR_LIME ((gcolor_Color){.r = 0, .g = 158, .b = 47, .a = 255})
#define GCOLOR_MAGENTA ((gcolor_Color){.r = 255, .g = 0, .b = 255, .a = 255})
#define GCOLOR_MAROON ((gcolor_Color){.r = 190, .g = 33, .b = 55, .a = 255})
#define GCOLOR_ORANGE ((gcolor_Color){.r = 255, .g = 161, .b = 0, .a = 255})
#define GCOLOR_PINK ((gcolor_Color){.r = 255, .g = 109, .b = 194, .a = 255})
#define GCOLOR_PURPLE ((gcolor_Color){.r = 200, .g = 122, .b = 255, .a = 255})
#define GCOLOR_RED ((gcolor_Color){.r = 230, .g = 41, .b = 55, .a = 255})
#define GCOLOR_SKYBLUE ((gcolor_Color){.r = 102, .g = 191, .b = 255, .a = 255})
#define GCOLOR_VIOLET ((gcolor_Color){.r = 135, .g = 60, .b = 190, .a = 255})
#define GCOLOR_WHITE ((gcolor_Color){.r = 255, .g = 255, .b = 255, .a = 255})
#define GCOLOR_YELLOW ((gcolor_Color){.r = 253, .g = 249, .b = 0, .a = 255})

static inline gvec2_Vec2 gvec2_add(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2){.x = a.x + b.x, .y = a.y + b.y};
}

static inline gvec2_Vec2 gvec2_sub(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2){.x = a.x - b.x, .y = a.y - b.y};
}

static inline gvec2_Vec2 gvec2_mul(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2){.x = a.x * b.x, .y = a.y * b.y};
}

static inline gvec2_Vec2 gvec2_div(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2){.x = a.x / b.x, .y = a.y / b.y};
}

static inline gvec2_Vec2 gvec2_scale(gvec2_Vec2 v, float factor) {
    return (gvec2_Vec2){.x = v.x * factor, .y = v.y * factor};
}

static inline float gvec2_length(gvec2_Vec2 v) {
    return hypotf(v.x, v.y);
}

/* Normalizes into *out; returns false for a zero-length vector, leaving *out
 * untouched. */
static inline bool gvec2_normalize(gvec2_Vec2 v, gvec2_Vec2 *out) {
    float length = gvec2_length(v);
    if (length == 0.0f || !out) {
        return false;
    }
    *out = (gvec2_Vec2){.x = v.x / length, .y = v.y / length};
    return true;
}

#ifdef __cplusplus
}
#endif
#endif // GTYPES_TYPES_H

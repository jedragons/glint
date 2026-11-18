#pragma once

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gvec2_Vec2 {
    float x;
    float y;
} gvec2_Vec2;

static inline gvec2_Vec2 gvec2_add(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2) {.x = a.x + b.x, .y = a.y + b.y};
}

static inline gvec2_Vec2 gvec2_sub(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2) {.x = a.x - b.x, .y = a.y - b.y};
}

static inline gvec2_Vec2 gvec2_mul(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2) {.x = a.x * b.x, .y = a.y * b.y};
}

static inline gvec2_Vec2 gvec2_div(gvec2_Vec2 a, gvec2_Vec2 b) {
    return (gvec2_Vec2) {.x = a.x / b.x, .y = a.y / b.y};
}

static inline gvec2_Vec2 gvec2_scale(gvec2_Vec2 v, float factor) {
    return (gvec2_Vec2) {.x = v.x * factor, .y = v.y * factor};
}

static inline float gvec2_length(gvec2_Vec2 v) {
    return hypotf(v.x, v.y);
}

/// Normalize into `out`.
///
/// # Errors
///
/// Returns false for zero length or NULL output, leaving `out` unchanged.
static inline bool gvec2_normalize(gvec2_Vec2 v, gvec2_Vec2* out) {
    float length = gvec2_length(v);
    if (length == 0.0f || out == NULL) {
        return false;
    }
    *out = (gvec2_Vec2) {.x = v.x / length, .y = v.y / length};
    return true;
}

#ifdef __cplusplus
}
#endif

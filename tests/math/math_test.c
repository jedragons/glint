#include <glint/math/color.h>
#include <glint/math/vec2.h>
#include <glint/unit/unit.h>

#include <math.h>

static bool gmath__test_color_matches(gcolor_Color a, gcolor_Color b);

GUNIT_TEST(named_colors,
    gunit_assert("black is opaque", gmath__test_color_matches(GCOLOR_BLACK, (gcolor_Color) {.a = 255}));
    gunit_assert("blank is transparent black", gmath__test_color_matches(GCOLOR_BLANK, (gcolor_Color) {0}));
    gunit_assert("white is opaque", gmath__test_color_matches(GCOLOR_WHITE, (gcolor_Color) {255, 255, 255, 255}));
)

GUNIT_TEST(color_from_hex,
    gunit_assert(
        "hex splits channels",
        gmath__test_color_matches(gcolor_from_hex(0x4a2b1cffu), (gcolor_Color) {74, 43, 28, 255})
    );
    gunit_assert(
        "transparent hex",
        gmath__test_color_matches(gcolor_from_hex(0xff000000u), (gcolor_Color) {255, 0, 0, 0})
    );
    gunit_assert(
        "hex preserves all channels",
        gmath__test_color_matches(gcolor_from_hex(0x12345678u), (gcolor_Color) {18, 52, 86, 120})
    );
)

GUNIT_TEST(vec2_add,
    gvec2_Vec2 a = {.x = 3.0f, .y = 4.0f};
    gvec2_Vec2 b = {.x = 1.5f, .y = -2.0f};
    gvec2_Vec2 result = gvec2_add(a, b);
    gunit_assert("component-wise add", result.x == 4.5f && result.y == 2.0f);
)

GUNIT_TEST(vec2_subtract,
    gvec2_Vec2 a = {.x = 3.0f, .y = 4.0f};
    gvec2_Vec2 b = {.x = 1.5f, .y = -2.0f};
    gvec2_Vec2 result = gvec2_sub(a, b);
    gunit_assert("component-wise subtract", result.x == 1.5f && result.y == 6.0f);
)

GUNIT_TEST(vec2_multiply,
    gvec2_Vec2 a = {.x = 3.0f, .y = 4.0f};
    gvec2_Vec2 b = {.x = 1.5f, .y = -2.0f};
    gvec2_Vec2 result = gvec2_mul(a, b);
    gunit_assert("component-wise multiply", result.x == 4.5f && result.y == -8.0f);
)

GUNIT_TEST(vec2_divide,
    gvec2_Vec2 result = gvec2_div((gvec2_Vec2) {3, 4}, (gvec2_Vec2) {2, 8});
    gunit_assert("component-wise division", result.x == 1.5f && result.y == 0.5f);
)

GUNIT_TEST(vec2_scale,
    gvec2_Vec2 result = gvec2_scale((gvec2_Vec2) {3, 4}, 2.0f);
    gunit_assert("scalar multiplication", result.x == 6.0f && result.y == 8.0f);
)

GUNIT_TEST(vec2_scale_by_zero,
    gvec2_Vec2 result = gvec2_scale((gvec2_Vec2) {3, 4}, 0.0f);
    gunit_assert("scale by zero", result.x == 0.0f && result.y == 0.0f);
)

GUNIT_TEST(vec2_length,
    float length = gvec2_length((gvec2_Vec2) {3, 4});
    gunit_assert("3-4 vector has length 5", fabsf(length - 5.0f) < 1e-6f);
)

GUNIT_TEST(vec2_zero_length,
    gunit_assert("zero vector length", gvec2_length((gvec2_Vec2) {0}) == 0.0f);
)

GUNIT_TEST(vec2_normalize,
    gvec2_Vec2 unit = {.x = 42.0f, .y = 42.0f};
    gunit_assert("normalize succeeds", gvec2_normalize((gvec2_Vec2) {3, 4}, &unit));
    gunit_assert("normalized components", unit.x == 0.6f && unit.y == 0.8f);
    float length = gvec2_length(unit);
    gunit_assert("normalized vector has unit length", fabsf(length - 1.0f) < 1e-6f);
)

GUNIT_TEST(vec2_normalize_rejects_zero,
    gvec2_Vec2 unit = {.x = 7.0f, .y = 7.0f};
    gunit_assert("zero vector rejected", !gvec2_normalize((gvec2_Vec2) {0}, &unit));
    gunit_assert("rejected normalization leaves output untouched", unit.x == 7.0f && unit.y == 7.0f);
)

GUNIT_TEST(vec2_normalize_rejects_null_output,
    gunit_assert("null output rejected", !gvec2_normalize((gvec2_Vec2) {3, 4}, NULL));
)

gunit_main();

static bool gmath__test_color_matches(gcolor_Color a, gcolor_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

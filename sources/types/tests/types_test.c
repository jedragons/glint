#include <glint/types.h>
#include <mewunit.h>

static bool gtypes__test_color_matches(gcolor_Color a, gcolor_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

TEST(named_colors, {
    mewassert("black", gtypes__test_color_matches(GCOLOR_BLACK, (gcolor_Color){0, 0, 0, 255}));
    mewassert("blank", gtypes__test_color_matches(GCOLOR_BLANK, (gcolor_Color){0, 0, 0, 0}));
    mewassert("white", gtypes__test_color_matches(GCOLOR_WHITE, (gcolor_Color){255, 255, 255, 255}));
})

TEST(color_from_hex, {
    mewassert("hex splits channels", gtypes__test_color_matches(gcolor_from_hex(0x4a2b1cffu), (gcolor_Color){74, 43, 28, 255}));
    mewassert("transparent hex", gtypes__test_color_matches(gcolor_from_hex(0xff000000u), (gcolor_Color){255, 0, 0, 0}));
    mewassert("hex preserves all channels", gtypes__test_color_matches(gcolor_from_hex(0x12345678u), (gcolor_Color){18, 52, 86, 120}));
})

TEST(vec2_arithmetic, {
    gvec2_Vec2 a = {.x = 3.0f, .y = 4.0f};
    gvec2_Vec2 b = {.x = 1.5f, .y = -2.0f};
    mewassert("add", gvec2_add(a, b).x == 4.5f && gvec2_add(a, b).y == 2.0f);
    mewassert("sub", gvec2_sub(a, b).x == 1.5f && gvec2_sub(a, b).y == 6.0f);
    mewassert("mul", gvec2_mul(a, b).x == 4.5f && gvec2_mul(a, b).y == -8.0f);
    mewassert("div", gvec2_div(a, (gvec2_Vec2){.x = 2.0f, .y = 8.0f}).x == 1.5f &&
               gvec2_div(a, (gvec2_Vec2){.x = 2.0f, .y = 8.0f}).y == 0.5f);
    mewassert("scale", gvec2_scale(a, 2.0f).x == 6.0f && gvec2_scale(a, 2.0f).y == 8.0f);
    mewassert("scale by zero", gvec2_scale(a, 0.0f).x == 0.0f && gvec2_scale(a, 0.0f).y == 0.0f);
})

TEST(vec2_length_normalize, {
    gvec2_Vec2 v = {.x = 3.0f, .y = 4.0f};
    mewassert("3-4-5 length", gvec2_length(v) - 5.0f < 1e-6f && 5.0f - gvec2_length(v) < 1e-6f);
    mewassert("zero vector length", gvec2_length((gvec2_Vec2){.x = 0.0f, .y = 0.0f}) == 0.0f);

    gvec2_Vec2 unit = {.x = 42.0f, .y = 42.0f};
    mewassert("normalize succeeds", gvec2_normalize(v, &unit));
    mewassert("normalized components", unit.x == 0.6f && unit.y == 0.8f);
    mewassert("normalized length", gvec2_length(unit) - 1.0f < 1e-6f && 1.0f - gvec2_length(unit) < 1e-6f);

    unit = (gvec2_Vec2){.x = 7.0f, .y = 7.0f};
    mewassert("zero vector is rejected", !gvec2_normalize((gvec2_Vec2){.x = 0.0f, .y = 0.0f}, &unit));
    mewassert("rejected normalize leaves out untouched", unit.x == 7.0f && unit.y == 7.0f);
    mewassert("null out is rejected", !gvec2_normalize(v, NULL));
})

mewtest_main();

#include <glint/core/alloc.h>
#include <glint/modules/types/bindings.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

typedef struct gtypes__TestContext {
    galloc_Allocator alloc;
    lua_State* lua;
} gtypes__TestContext;

static bool gtypes__test_create(gtypes__TestContext* context);
static void gtypes__test_free(void* user);
static bool gtypes__test_run(gtypes__TestContext* context, const char* source);

GUNIT_TEST(vector_arithmetic,
    gtypes__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtypes__test_free, &context);
        gunit_assert("bind vector type", gtypes__test_create(&context));
        const char* source =
            "left = Vec2.new(6, 8)\n"
            "right = Vec2.new(2, 4)\n";
        gunit_assert("create vector operands", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("adds corresponding components", {
        const char* source =
            "local sum = left + right\n"
            "assert(sum.x == 8 and sum.y == 12)\n";
        gunit_assert("add vectors", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("subtracts corresponding components", {
        const char* source =
            "local difference = left - right\n"
            "assert(difference.x == 4 and difference.y == 4)\n";
        gunit_assert("subtract vectors", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("multiplies corresponding components", {
        const char* source =
            "local product = left * right\n"
            "assert(product.x == 12 and product.y == 32)\n";
        gunit_assert("multiply vectors", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("divides corresponding components", {
        const char* source =
            "local quotient = left / right\n"
            "assert(quotient.x == 3 and quotient.y == 2)\n";
        gunit_assert("divide vectors", gtypes__test_run(&context, source));
    })
)

GUNIT_TEST(vector_values,
    gtypes__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtypes__test_free, &context);
        gunit_assert("bind vector type", gtypes__test_create(&context));
    })

    GUNIT_SECTION("starts at the origin without constructor arguments", {
        const char* source =
            "local vector = Vec2.new()\n"
            "assert(vector.x == 0 and vector.y == 0)\n";
        gunit_assert("construct a default vector", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("allows a component to be reassigned", {
        const char* source =
            "local vector = Vec2.new(6, 8)\n"
            "vector.x = 7\n"
            "assert(vector.x == 7)\n";
        gunit_assert("change the x component", gtypes__test_run(&context, source));
    })
)

GUNIT_TEST(vector_methods,
    gtypes__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtypes__test_free, &context);
        gunit_assert("bind vector type", gtypes__test_create(&context));
        const char* source =
            "vector = Vec2.new(6, 8)\n"
            "zero = Vec2.new()\n";
        gunit_assert("create vectors", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("scales a copy", {
        const char* source =
            "local scaled = vector:scale(0.5)\n"
            "assert(scaled.x == 3 and scaled.y == 4)\n"
            "assert(vector.x == 6)\n";
        gunit_assert("scale a vector without changing it", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("returns the length", {
        const char* source =
            "assert(vector:length() == 10)\n"
            "assert(zero:length() == 0)\n";
        gunit_assert("measure vector length", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("normalizes a nonzero vector into a copy", {
        const char* source =
            "local unit = vector:normalize()\n"
            "assert(math.abs(unit.x - 0.6) < 1e-6)\n"
            "assert(math.abs(unit.y - 0.8) < 1e-6)\n"
            "assert(math.abs(unit:length() - 1) < 1e-6)\n"
            "assert(vector.x == 6 and vector.y == 8)\n";
        gunit_assert("normalize a vector without changing it", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("cannot normalize a zero vector", {
        gunit_assert("zero has no unit vector", gtypes__test_run(&context, "assert(zero:normalize() == nil)"));
    })
)

GUNIT_TEST(rectangle_construction,
    gtypes__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtypes__test_free, &context);
        gunit_assert("bind rectangle type", gtypes__test_create(&context));
    })

    GUNIT_SECTION("defaults to zero position and size", {
        const char* source =
            "local rect = Rect.new()\n"
            "assert(rect.x == 0 and rect.y == 0)\n"
            "assert(rect.w == 0 and rect.h == 0)\n";
        gunit_assert("construct a default rectangle", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("takes an explicit position and size", {
        const char* source =
            "local rect = Rect.new(1, 2, 3, 4)\n"
            "assert(rect.x == 1 and rect.y == 2)\n"
            "assert(rect.w == 3 and rect.h == 4)\n";
        gunit_assert("construct a rectangle with explicit components", gtypes__test_run(&context, source));
    })
)

GUNIT_TEST(color_construction,
    gtypes__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtypes__test_free, &context);
        gunit_assert("bind color type", gtypes__test_create(&context));
    })

    GUNIT_SECTION("defaults to transparent black", {
        const char* source =
            "local color = Color.new()\n"
            "assert(color.r == 0 and color.g == 0 and color.b == 0)\n"
            "assert(color.a == 0)\n";
        gunit_assert("construct a default color", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("unpacks an RGBA integer", {
        const char* source =
            "local color = Color.new(0x12345678)\n"
            "assert(color.r == 18 and color.g == 52 and color.b == 86)\n"
            "assert(color.a == 120)\n";
        gunit_assert("construct a color from a packed value", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("defaults to opaque when given RGB components", {
        const char* source =
            "local color = Color.new(1, 2, 3)\n"
            "assert(color.r == 1 and color.g == 2 and color.b == 3)\n"
            "assert(color.a == 255)\n";
        gunit_assert("construct a color with implicit alpha", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("uses the supplied alpha component", {
        const char* source =
            "local color = Color.new(1, 2, 3, 4)\n"
            "assert(color.a == 4)\n";
        gunit_assert("construct a color with explicit alpha", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("rejects a component above 255", {
        const char* source =
            "local constructed = pcall(function() Color.new(256, 0, 0) end)\n"
            "assert(not constructed)\n";
        gunit_assert("reject an out-of-range red component", gtypes__test_run(&context, source));
    })
)

GUNIT_TEST(named_color_values,
    gtypes__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gtypes__test_free, &context);
        gunit_assert("bind color type", gtypes__test_create(&context));
    })

    GUNIT_SECTION("exposes the named constants", {
        const char* source =
            "local red = Color.red\n"
            "assert(red.r == 230 and red.g == 41 and red.b == 55 and red.a == 255)\n"
            "assert(Color.blank.a == 0)\n"
            "assert(Color.white.r == 255 and Color.black.r == 0)\n";
        gunit_assert("read named colors", gtypes__test_run(&context, source));
    })

    GUNIT_SECTION("returns an independent copy of each constant", {
        const char* source =
            "local red = Color.red\n"
            "local original = red.r\n"
            "red.r = (original + 1) % 256\n"
            "assert(Color.red.r == original)\n";
        gunit_assert("change a copy of a named color", gtypes__test_run(&context, source));
    })
)

gunit_main();

static bool gtypes__test_create(gtypes__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->lua = luaL_newstate();
    if (!gunit_check("create Lua state", context->lua != NULL)) {
        return false;
    }
    luaL_openlibs(context->lua);
    return gunit_check("register types", gtypes_bindings_bind(context->alloc, context->lua));
}

static void gtypes__test_free(void* user) {
    gtypes__TestContext* context = user;
    if (context->lua != NULL) {
        lua_close(context->lua);
    }
}

static bool gtypes__test_run(gtypes__TestContext* context, const char* source) {
    if (luaL_dostring(context->lua, source) == LUA_OK) {
        return true;
    }

    const char* message = lua_tostring(context->lua, -1);
    return gunit_check(message != NULL ? message : "Lua chunk failed", false);
}

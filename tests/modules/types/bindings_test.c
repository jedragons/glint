#include <glint/core/alloc.h>
#include <glint/modules/types/bindings.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>

typedef struct gtypes__TestContext {
    galloc_Allocator alloc;
    lua_State* lua;
} gtypes__TestContext;

static bool gtypes__test_create(gtypes__TestContext* context);
static void gtypes__test_free(void* user);
static bool gtypes__test_run(gtypes__TestContext* context, const char* source);

GUNIT_TEST(vector_defaults,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert("test helper succeeds", gtypes__test_run(&context, "local z=Vec2.new(); assert(z.x==0 and z.y==0)"));
)

GUNIT_TEST(vector_addition,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local c=Vec2.new(6,8)+Vec2.new(2,4); assert(c.x==8 and c.y==12)")
    );
)

GUNIT_TEST(vector_subtraction,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local c=Vec2.new(6,8)-Vec2.new(2,4); assert(c.x==4 and c.y==4)")
    );
)

GUNIT_TEST(vector_multiplication,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local c=Vec2.new(6,8)*Vec2.new(2,4); assert(c.x==12 and c.y==32)")
    );
)

GUNIT_TEST(vector_division,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local c=Vec2.new(6,8)/Vec2.new(2,4); assert(c.x==3 and c.y==2)")
    );
)

GUNIT_TEST(vector_scaling_preserves_source,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local a=Vec2.new(6,8); local c=a:scale(.5); assert(c.x==3 and c.y==4 and a.x==6)")
    );
)

GUNIT_TEST(vector_length,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "assert(Vec2.new(6,8):length()==10 and Vec2.new():length()==0)")
    );
)

GUNIT_TEST(zero_vector_cannot_normalize,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert("test helper succeeds", gtypes__test_run(&context, "assert(Vec2.new():normalize()==nil)"));
)

GUNIT_TEST(vector_normalization_preserves_source,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(
            &context,
            "local a=Vec2.new(6,8); local c=a:normalize(); assert(math.abs(c.x-.6)<1e-6 and math.abs(c.y-.8)<1e-6); assert(math.abs(c:length()-1)<1e-6 and a.x==6 and a.y==8)"
        )
    );
)

GUNIT_TEST(vector_fields_are_mutable,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert("test helper succeeds", gtypes__test_run(&context, "local a=Vec2.new(6,8); a.x=7; assert(a.x==7)"));
)

GUNIT_TEST(rectangle_defaults,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local r=Rect.new(); assert(r.x==0 and r.y==0 and r.w==0 and r.h==0)")
    );
)

GUNIT_TEST(rectangle_components,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local r=Rect.new(1,2,3,4); assert(r.x==1 and r.y==2 and r.w==3 and r.h==4)")
    );
)

GUNIT_TEST(color_defaults,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local c=Color.new(); assert(c.r==0 and c.g==0 and c.b==0 and c.a==0)")
    );
)

GUNIT_TEST(color_packed_value,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(
            &context,
            "local c=Color.new(0x12345678); assert(c.r==18 and c.g==52 and c.b==86 and c.a==120)"
        )
    );
)

GUNIT_TEST(color_components_default_alpha,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "local c=Color.new(1,2,3); assert(c.r==1 and c.g==2 and c.b==3 and c.a==255)")
    );
)

GUNIT_TEST(color_components_explicit_alpha,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert("test helper succeeds", gtypes__test_run(&context, "local c=Color.new(1,2,3,4); assert(c.a==4)"));
)

GUNIT_TEST(color_named_values,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(
            &context,
            "assert(Color.red.r==230 and Color.red.g==41 and Color.red.b==55 and Color.red.a==255); assert(Color.blank.a==0 and Color.white.r==255 and Color.black.r==0)"
        )
    );
)

GUNIT_TEST(color_named_values_are_copied,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(
            &context,
            "local red=Color.red; local original=red.r; red.r=(original+1)%256; assert(Color.red.r==original)"
        )
    );
)

GUNIT_TEST(color_rejects_out_of_range_component,
    gtypes__TestContext context = {0};
    gunit_defer(gtypes__test_free, &context);
    gunit_assert("create test context", gtypes__test_create(&context));
    gunit_assert(
        "test helper succeeds",
        gtypes__test_run(&context, "assert(not pcall(function() Color.new(256,0,0) end))")
    );
)

gunit_main();

static bool gtypes__test_create(gtypes__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->lua = luaL_newstate();
    if (!gunit_check("create Lua state", context->lua != NULL)) {
        return false;
    }
    luaL_openlibs(context->lua);
    if (!gunit_check("register types", gtypes_bindings_bind(context->alloc, context->lua))) {
        return false;
    }
    return true;
}

static void gtypes__test_free(void* user) {
    gtypes__TestContext* context = user;
    if (context->lua != NULL) {
        lua_close(context->lua);
    }
}

static bool gtypes__test_run(gtypes__TestContext* context, const char* source) {
    if (luaL_dostring(context->lua, source) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(context->lua, -1));
        if (!gunit_check("Lua type behavior succeeds", false)) {
            return false;
        }
    }
    return true;
}

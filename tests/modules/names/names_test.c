#include <glint/modules/names/names.h>
#include <glint/unit/unit.h>

#include <string.h>

typedef struct gname__TestContext {
    galloc_Allocator alloc;
    gecs_World* world;
    gecs_Entity first;
    gecs_Entity second;
} gname__TestContext;

static bool gname__test_create(gname__TestContext* context);
static void gname__test_free(void* user);
static bool gname__test_has_name(gecs_Entity entity, const char* expected);

GUNIT_TEST(unnamed_entity,
    gname__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gname__test_free, &ctx);
        gunit_assert("spawn two entities", gname__test_create(&ctx));
    })

    GUNIT_SECTION("setting a name makes it available for lookup", {
        gunit_assert("unassigned name is absent", !gname_exists(ctx.world, "hero"));
        gunit_assert("null name is absent", !gname_exists(ctx.world, NULL));

        gunit_assert("assign hero", gname_set(ctx.first, "hero"));

        gunit_assert("assigned name exists", gname_exists(ctx.world, "hero"));
        gunit_assert("lookup finds the named entity", gname_find(ctx.world, "hero").id == ctx.first.id);
        gunit_assert("other names remain absent", !gname_exists(ctx.world, "villain"));
    })

    GUNIT_SECTION("renaming requires an existing name", {
        gunit_assert("unnamed entity cannot be renamed", !gname_rename(ctx.first, "hero"));
        gunit_assert("null name is rejected", !gname_rename(ctx.first, NULL));
    })

    GUNIT_SECTION("there is no name to remove", {
        gunit_assert("removal reports no name", !gname_remove(ctx.first));
    })

    GUNIT_SECTION("a destroyed entity cannot be renamed", {
        gecs_Entity_destroy(ctx.first);
        gunit_assert("dead entity cannot be renamed", !gname_rename(ctx.first, "zombie"));
    })
)

GUNIT_TEST(name_assignment,
    gname__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gname__test_free, &ctx);
        gunit_assert("spawn two entities", gname__test_create(&ctx));
        gunit_assert("name the first entity", gname_set(ctx.first, "hero"));
    })

    GUNIT_SECTION("a second entity cannot take the name", {
        gunit_assert("duplicate rejected", !gname_set(ctx.second, "hero"));
    })

    GUNIT_SECTION("a new assignment replaces the old name", {
        gunit_assert("assign a replacement", gname_set(ctx.first, "player"));
        gunit_assert("old name removed", gname_find(ctx.world, "hero").id == 0);
        gunit_assert("new name visible", gname__test_has_name(ctx.first, "player"));
    })

    GUNIT_SECTION("assigning NULL removes the name", {
        gunit_assert("remove by assignment", gname_set(ctx.first, NULL));
        gunit_assert("entity has no name", gname_get(ctx.first) == NULL);
    })

    GUNIT_SECTION("destroying the entity frees its name", {
        gecs_Entity_destroy(ctx.first);
        gunit_assert("destroyed name absent", !gname_exists(ctx.world, "hero"));
        gunit_assert("name may be reused", gname_set(ctx.second, "hero"));
    })
)

GUNIT_TEST(name_rename,
    gname__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gname__test_free, &ctx);
        gunit_assert("spawn two entities", gname__test_create(&ctx));
        gunit_assert("name the first entity", gname_set(ctx.first, "hero"));
    })

    GUNIT_SECTION("renaming to the current name succeeds", {
        gunit_assert("same name accepted", gname_rename(ctx.first, "hero"));
        gunit_assert("name unchanged", gname__test_has_name(ctx.first, "hero"));
    })

    GUNIT_SECTION("a taken name leaves both entities unchanged", {
        gunit_assert("name the second entity", gname_set(ctx.second, "villain"));
        gunit_assert("taken name rejected", !gname_rename(ctx.second, "hero"));

        gunit_assert("first name preserved", gname__test_has_name(ctx.first, "hero"));
        gunit_assert("second name preserved", gname__test_has_name(ctx.second, "villain"));
        gunit_assert("first lookup preserved", gname_find(ctx.world, "hero").id == ctx.first.id);
        gunit_assert("second lookup preserved", gname_find(ctx.world, "villain").id == ctx.second.id);
    })

    GUNIT_SECTION("renaming updates lookup and frees the old name for reuse", {
        gunit_assert("rename the first entity", gname_rename(ctx.first, "player"));
        gunit_assert("old name free", !gname_exists(ctx.world, "hero"));
        gunit_assert("first entity has its new name", gname__test_has_name(ctx.first, "player"));
        gunit_assert("new name finds the first entity", gname_find(ctx.world, "player").id == ctx.first.id);

        gunit_assert("unnamed entity cannot rename to the freed name", !gname_rename(ctx.second, "hero"));
        gunit_assert("name the second entity", gname_set(ctx.second, "sidekick"));
        gunit_assert("rename the second entity to the freed name", gname_rename(ctx.second, "hero"));
        gunit_assert("second entity has the freed name", gname__test_has_name(ctx.second, "hero"));
        gunit_assert("previous second name is gone", !gname_exists(ctx.world, "sidekick"));
    })
)

GUNIT_TEST(name_removal,
    gname__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gname__test_free, &ctx);
        gunit_assert("spawn two entities", gname__test_create(&ctx));
        gunit_assert("name the first entity", gname_set(ctx.first, "hero"));
        gunit_assert("remove its name", gname_remove(ctx.first));
    })

    GUNIT_SECTION("removal clears lookup and cannot be repeated", {
        gunit_assert("removal not repeatable", !gname_remove(ctx.first));
        gunit_assert("name absent from lookup", !gname_exists(ctx.world, "hero"));
        gunit_assert("entity has no name", gname_get(ctx.first) == NULL);
    })

    GUNIT_SECTION("destroying the former owner preserves the new owner's name", {
        gunit_assert("reuse the name", gname_set(ctx.second, "hero"));
        gecs_Entity_destroy(ctx.first);
        gunit_assert("dead entity cannot remove a name", !gname_remove(ctx.first));
        gunit_assert("name still exists", gname_exists(ctx.world, "hero"));
        gunit_assert("name belongs to the second entity", gname_find(ctx.world, "hero").id == ctx.second.id);
    })
)

gunit_main();

static bool gname__test_create(gname__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->world = gecs_World_new(context->alloc);
    if (!gunit_check("world created", context->world != NULL)) {
        return false;
    }
    context->first = gecs_spawn(context->world);
    context->second = gecs_spawn(context->world);
    if (!gunit_check("spawn first entity", context->first.id != 0)) {
        return false;
    }
    return gunit_check("spawn second entity", context->second.id != 0);
}

static void gname__test_free(void* user) {
    gname__TestContext* context = user;
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
}

static bool gname__test_has_name(gecs_Entity entity, const char* expected) {
    const char* actual = gname_get(entity);
    return actual != NULL && strcmp(actual, expected) == 0;
}

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

GUNIT_TEST(assigned_name_can_be_looked_up, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name assigned", gname_set(context.first, "hero"));
    gunit_assert("lookup finds entity", gname_find(context.world, "hero").id == context.first.id);
    gunit_return();
})

GUNIT_TEST(duplicate_names_are_rejected, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("duplicate rejected", !gname_set(context.second, "hero"));
    gunit_return();
})

GUNIT_TEST(assigning_new_name_replaces_old_name, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("change name", gname_set(context.first, "player"));
    gunit_assert("old name removed", gname_find(context.world, "hero").id == 0);
    gunit_assert("new name visible", strcmp(gname_get(context.first), "player") == 0);
    gunit_return();
})

GUNIT_TEST(destroyed_entity_name_can_be_reused, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gecs_Entity_destroy(context.first);
    gunit_assert("destroyed name absent", !gname_exists(context.world, "hero"));
    gunit_assert("name may be reused", gname_set(context.second, "hero"));
    gunit_return();
})

GUNIT_TEST(null_assignment_removes_name, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("remove by assignment", gname_set(context.first, NULL));
    gunit_assert("removed name absent", gname_get(context.first) == NULL);
    gunit_return();
})

GUNIT_TEST(name_existence_checks, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("unset name absent", !gname_exists(context.world, "hero"));
    gunit_assert("null name absent", !gname_exists(context.world, NULL));
    gunit_assert("name entity", gname_set(context.first, "hero"));
    gunit_assert("assigned name exists", gname_exists(context.world, "hero"));
    gunit_assert("other names absent", !gname_exists(context.world, "villain"));
    gunit_return();
})

GUNIT_TEST(unnamed_entity_cannot_rename, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("unnamed cannot rename", !gname_rename(context.first, "hero"));
    gunit_assert("null name cannot rename", !gname_rename(context.first, NULL));
    gunit_return();
})

GUNIT_TEST(rename_cannot_take_existing_name, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("name second", gname_set(context.second, "villain"));
    gunit_assert("taken name rejected", !gname_rename(context.second, "hero"));
    gunit_assert("first name preserved", strcmp(gname_get(context.first), "hero") == 0);
    gunit_assert("second name preserved", strcmp(gname_get(context.second), "villain") == 0);
    gunit_assert("first lookup preserved", gname_find(context.world, "hero").id == context.first.id);
    gunit_assert("second lookup preserved", gname_find(context.world, "villain").id == context.second.id);
    gunit_return();
})

GUNIT_TEST(rename_updates_lookup, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("rename succeeds", gname_rename(context.first, "player"));
    gunit_assert("old name free", !gname_exists(context.world, "hero"));
    gunit_assert(
        "new name finds entity",
        strcmp(gname_get(context.first), "player") == 0 && gname_find(context.world, "player").id == context.first.id
    );
    gunit_return();
})

GUNIT_TEST(rename_to_same_name_is_idempotent, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("same name accepted", gname_rename(context.first, "hero"));
    gunit_assert("name unchanged", strcmp(gname_get(context.first), "hero") == 0);
    gunit_return();
})

GUNIT_TEST(rename_can_reuse_freed_name, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("rename first", gname_rename(context.first, "player"));
    gunit_assert("unnamed cannot take freed name", !gname_rename(context.second, "hero"));
    gunit_assert("name second", gname_set(context.second, "sidekick"));
    gunit_assert("rename second into freed name", gname_rename(context.second, "hero"));
    gunit_assert(
        "old second name gone",
        strcmp(gname_get(context.second), "hero") == 0 && !gname_exists(context.world, "sidekick")
    );
    gunit_return();
})

GUNIT_TEST(dead_entity_cannot_rename, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gecs_Entity_destroy(context.first);
    gunit_assert("dead entity cannot rename", !gname_rename(context.first, "zombie"));
    gunit_return();
})

GUNIT_TEST(unnamed_entity_cannot_remove_name, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("unnamed cannot remove", !gname_remove(context.first));
    gunit_return();
})

GUNIT_TEST(name_removal_clears_lookup_once, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("remove name", gname_remove(context.first));
    gunit_assert("removal not repeatable", !gname_remove(context.first));
    gunit_assert("removed name absent", !gname_exists(context.world, "hero") && gname_get(context.first) == NULL);
    gunit_return();
})

GUNIT_TEST(name_removal_does_not_affect_other_entities, {
    gname__TestContext context = {0};
    gunit_defer(gname__test_free, &context);
    gunit_assert("create test context", gname__test_create(&context));
    gunit_assert("name first", gname_set(context.first, "hero"));
    gunit_assert("remove name", gname_remove(context.first));
    gunit_assert("reuse name", gname_set(context.second, "hero"));
    gecs_Entity_destroy(context.first);
    gunit_assert("dead entity cannot remove", !gname_remove(context.first));
    gunit_assert("other entity retains name", gname_exists(context.world, "hero"));
    gunit_return();
})

gunit_main();

static bool gname__test_create(gname__TestContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->world = gecs_World_new(context->alloc);
    if (!gunit_check("world created", context->world != NULL)) {
        return false;
    }
    context->first = gecs_spawn(context->world);
    context->second = gecs_spawn(context->world);
    if (!gunit_check("entities spawned", context->first.id != 0 && context->second.id != 0)) {
        return false;
    }
    return true;
}

static void gname__test_free(void* user) {
    gname__TestContext* context = user;
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
}

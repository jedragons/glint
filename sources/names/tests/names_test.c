#include <glint/names.h>
#include <mewunit.h>
#include <string.h>

TEST(entity_names, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    mewassert("world was created", world);
    gecs_Entity first = gecs_Entity_new(alloc, world);
    gecs_Entity second = gecs_Entity_new(alloc, world);
    mewassert("name was assigned", gname_set(alloc, world, first, "hero"));
    mewassert("name lookup found the entity", gname_find(world, "hero") == first);
    mewassert("duplicate names are rejected", !gname_set(alloc, world, second, "hero"));
    mewassert("name was changed", gname_set(alloc, world, first, "player"));
    mewassert("old name was removed", !gname_find(world, "hero"));
    mewassert("new name is visible", strcmp(gname_get(world, first), "player") == 0);
    gecs_Entity_destroy(world, first);
    mewassert("destroyed entities are no longer found", !gname_find(world, "player"));
    mewassert("removed name may be reused", gname_set(alloc, world, second, "player"));
    mewassert("name may be removed", gname_set(alloc, world, second, NULL));
    mewassert("removed name is absent", !gname_get(world, second));
    gecs_World_free(alloc, world);
})

TEST(entity_name_exists, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    mewassert("unset names do not exist", !gname_exists(world, "hero"));
    mewassert("Test expectation failed", gname_set(alloc, world, entity, "hero"));
    mewassert("set names exist", gname_exists(world, "hero"));
    mewassert("other names do not exist", !gname_exists(world, "villain"));
    mewassert("removed names do not exist", (gname_remove(world, entity), !gname_exists(world, "hero")));
    mewassert("null names never exist", !gname_exists(world, NULL));
    mewassert("existence survives other entities", (gname_set(alloc, world, entity, "hero"),
              gecs_Entity_destroy(world, entity), !gname_exists(world, "hero")));
    gecs_World_free(alloc, world);
})

TEST(entity_name_rename, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gecs_Entity first = gecs_Entity_new(alloc, world);
    gecs_Entity second = gecs_Entity_new(alloc, world);
    mewassert("unnamed entities cannot rename", !gname_rename(alloc, world, first, "hero"));
    mewassert("null names cannot rename", !gname_rename(alloc, world, first, NULL));
    mewassert("Test expectation failed", gname_set(alloc, world, first, "hero"));
    mewassert("taken names cannot rename", !gname_rename(alloc, world, second, "hero"));
    mewassert("renaming works", gname_rename(alloc, world, first, "player"));
    mewassert("old name is free", !gname_exists(world, "hero"));
    mewassert("entity answers to the new name", strcmp(gname_get(world, first), "player") == 0 &&
              gname_find(world, "player") == first);
    mewassert("renaming to the same name succeeds", gname_rename(alloc, world, first, "player"));
    mewassert("name unchanged", strcmp(gname_get(world, first), "player") == 0);
    mewassert("unnamed entities cannot take a freed name by rename", !gname_rename(alloc, world, second, "hero"));
    mewassert("Test expectation failed", gname_set(alloc, world, second, "sidekick"));
    mewassert("freed names may be renamed into", gname_rename(alloc, world, second, "hero"));
    mewassert("rename moved across entities", strcmp(gname_get(world, second), "hero") == 0 &&
              !gname_exists(world, "sidekick"));
    gecs_Entity_destroy(world, first);
    gecs_Entity_destroy(world, second);
    mewassert("dead entities cannot rename", !gname_rename(alloc, world, second, "zombie"));
    gecs_World_free(alloc, world);
})

TEST(entity_name_remove, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gecs_Entity first = gecs_Entity_new(alloc, world);
    gecs_Entity second = gecs_Entity_new(alloc, world);
    mewassert("unnamed entities cannot remove", !gname_remove(world, first));
    mewassert("Test expectation failed", gname_set(alloc, world, first, "hero"));
    mewassert("named entities remove", gname_remove(world, first));
    mewassert("removal is not repeatable", !gname_remove(world, first));
    mewassert("removed names are gone", !gname_exists(world, "hero") && !gname_get(world, first));
    mewassert("removed names free the entity", gname_set(alloc, world, second, "hero"));
    gecs_Entity_destroy(world, first);
    mewassert("dead entities cannot remove", !gname_remove(world, first));
    mewassert("other entities keep their names", gname_exists(world, "hero"));
    gecs_World_free(alloc, world);
})

mewtest_main();

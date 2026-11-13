#include <glint/alloc.h>
#include <glint/behaviors.h>
#include <glint/ecs.h>
#include <glint/script.h>
#include <glint/script_entity.h>
#include <glint/vfs.h>

#include <lua.h>
#include <mewunit.h>
#include <stdio.h>
#include <string.h>

static void gbeh__test_write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");
    mewassert("Test expectation failed", file);
    mewassert("Test expectation failed", fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    mewassert("Test expectation failed", fclose(file) == 0);
}

static void gbeh__test_run(galloc_Allocator alloc, gscript_Script *script, const char *source) {
    if (!gscript_run(alloc, script, source, strlen(source), "behavior test")) {
        fprintf(stderr, "%s\n", lua_tostring(gscript_state(script), -1));
        mewassert("Test expectation failed", 0);
    }
}

TEST(behaviors_test, {
    static const char *gbeh__test_parent_path = "glint-behavior-parent-test.lua";
    static const char *gbeh__test_child_path = "glint-behavior-child-test.lua";
    static const char *gbeh__test_replacement_path = "glint-behavior-replacement-test.lua";
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    gvfs_Vfs *vfs = gvfs_create(alloc);
    gbeh_Behaviors *behaviors;
    gecs_Entity parent;
    lua_State *lua;
    mewassert("Test expectation failed", world && script && vfs && gvfs_mount(alloc, vfs, "/", "."));
    gbeh__test_write_file(
        gbeh__test_parent_path,
        "return {setup=function(e) e.count=1 end, update=function(e) "
        "e.count=e.count+1; if not child then child=spawn('/glint-behavior-child-test.lua') end end, "
        "destroy=function(e) parent_destroyed=(parent_destroyed or 0)+1 end}"
    );
    gbeh__test_write_file(
        gbeh__test_child_path,
        "return {setup=function(e) e.started=true end, update=function(e) e:destroy() end, "
        "destroy=function(e) child_destroyed=(child_destroyed or 0)+1 end}"
    );
    gbeh__test_write_file(
        gbeh__test_replacement_path,
        "return {setup=function(e) e.replaced=true end, update=function(e) "
        "e.version=1 end, destroy=function(e) replacement_destroyed=true end}"
    );
    mewassert("Test expectation failed", gscript_bind_world(alloc, script, world));
    behaviors = gbeh_new(alloc, script, world, vfs);
    mewassert("Test expectation failed", behaviors);
    parent = gecs_Entity_new(alloc, world);
    mewassert(
        "Test expectation failed",
        parent && gbeh_attach(alloc, behaviors, parent, "/glint-behavior-parent-test.lua")
    );
    lua = gscript_state(script);
    gscript_push_entity(alloc, lua, world, parent);
    lua_setglobal(lua, "parent");
    mewassert("Test expectation failed", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(parent.count == 2 and child.started and child:valid())");
    mewassert("Test expectation failed", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(parent.count == 3 and not child:valid() and child_destroyed == 1)");
    gbeh__test_run(
        alloc,
        script,
        "parent:attach_module('/glint-behavior-replacement-test.lua'); assert(parent_destroyed == 1 and parent.replaced)"
    );
    mewassert("Test expectation failed", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(parent.version == 1)");
    gbeh__test_write_file(
        gbeh__test_replacement_path,
        "return {setup=function(e) error('reload must not call setup') end, "
        "update=function(e) e.version=200 end, destroy=function(e) reload_destroyed=true end}"
    );
    mewassert("Test expectation failed", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(parent.version == 200)");
    gbeh_destroy(alloc, behaviors);
    gbeh__test_run(alloc, script, "assert(reload_destroyed)");
    gscript_destroy(alloc, script);
    gvfs_destroy(alloc, vfs);
    gecs_World_free(alloc, world);
    mewassert("Test expectation failed", remove(gbeh__test_parent_path) == 0);
    mewassert("Test expectation failed", remove(gbeh__test_child_path) == 0);
    mewassert("Test expectation failed", remove(gbeh__test_replacement_path) == 0);
})

TEST(behavior_mutation_and_shutdown, {
    const char *a_file = "glint-mutate-a.lua";
    const char *b_file = "glint-mutate-b.lua";
    const char *c_file = "glint-mutate-c.lua";
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    gvfs_Vfs *vfs = gvfs_create(alloc);
    gbeh_Behaviors *behaviors;
    gecs_Entity a, b;
    lua_State *lua;
    mewassert("create environment", world && script && vfs && gvfs_mount(alloc, vfs, "/", "."));
    gbeh__test_write_file(
        a_file,
        "return {update=function(e) if not changed then changed=true; b:attach_module('/glint-mutate-c.lua') end end, destroy=function(e) shutdown_blocked=not pcall(function() spawn('/glint-mutate-c.lua') end) end}"
    );
    gbeh__test_write_file(b_file, "return {update=function(e) old_updated=true end, destroy=function(e) old_destroyed=true end}");
    gbeh__test_write_file(
        c_file,
        "return {setup=function(e) new_setup=true end, update=function(e) new_updates=(new_updates or 0)+1 end}"
    );
    mewassert("bind world", gscript_bind_world(alloc, script, world));
    behaviors = gbeh_new(alloc, script, world, vfs);
    a = gecs_Entity_new(alloc, world);
    b = gecs_Entity_new(alloc, world);
    mewassert(
        "attach behaviors",
        behaviors && a && b && gbeh_attach(alloc, behaviors, a, "/glint-mutate-a.lua")
            && gbeh_attach(alloc, behaviors, b, "/glint-mutate-b.lua")
    );
    lua = gscript_state(script);
    gscript_push_entity(alloc, lua, world, b);
    lua_setglobal(lua, "b");
    mewassert("update after replacing another behavior", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(old_destroyed and new_setup and not old_updated and not new_updates)");
    mewassert("next update", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(new_updates==1)");
    /* Invalid replacements retain the previous module and remain retryable. */
    gbeh__test_write_file(c_file, "return { update = function( invalid lua syntax");
    mewassert("bad reload fails", !gbeh_update(alloc, behaviors));
    lua_pop(lua, 1);
    gbeh__test_write_file(c_file, "return {update=function(e) new_updates=(new_updates or 0)+10 end}");
    mewassert("fixed reload succeeds", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(new_updates==11)");
    gbeh_destroy(alloc, behaviors);
    gbeh__test_run(alloc, script, "assert(shutdown_blocked)");
    gscript_destroy(alloc, script);
    gvfs_destroy(alloc, vfs);
    gecs_World_free(alloc, world);
    mewassert("remove fixture", remove(a_file) == 0 && remove(b_file) == 0 && remove(c_file) == 0);
})

typedef struct gbeh__TestFailAllocator {
    galloc_Allocator backing;
    bool fail_next;
    bool fail_all;
} gbeh__TestFailAllocator;

static void *gbeh__test_fail_alloc(void *data, size_t size) {
    gbeh__TestFailAllocator *state = data;
    if (state->fail_next || state->fail_all) {
        state->fail_next = false;
        return NULL;
    }
    return galloc_alloc(state->backing, size);
}

static void gbeh__test_fail_free(void *data, void *ptr) {
    gbeh__TestFailAllocator *state = data;
    galloc_free(state->backing, ptr);
}

static void *gbeh__test_fail_calloc(void *data, size_t count, size_t size) {
    gbeh__TestFailAllocator *state = data;
    if (state->fail_next || state->fail_all) {
        state->fail_next = false;
        return NULL;
    }
    return galloc_calloc(state->backing, count, size);
}

static void *gbeh__test_fail_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gbeh__TestFailAllocator *state = data;
    if (state->fail_next || state->fail_all) {
        state->fail_next = false;
        return NULL;
    }
    return galloc_realloc(state->backing, ptr, old_size, new_size);
}

TEST(behavior_update_allocation_failure, {
    static const galloc_FunctionTable gbeh__test_functions = {
        .alloc = gbeh__test_fail_alloc,
        .free = gbeh__test_fail_free,
        .calloc = gbeh__test_fail_calloc,
        .realloc = gbeh__test_fail_realloc,
    };
    gbeh__TestFailAllocator state = {.backing = galloc_malloc_allocator()};
    galloc_Allocator alloc = {.data = &state, .ftable = &gbeh__test_functions};
    const char *file = "glint-behavior-failure.lua";
    gecs_World *world = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    gvfs_Vfs *vfs = gvfs_create(alloc);
    gbeh_Behaviors *behaviors;
    gecs_Entity entity;
    mewassert("create environment", world && script && vfs && gvfs_mount(alloc, vfs, "/", "."));
    gbeh__test_write_file(file, "return {update=function(e) updates=(updates or 0)+1 end}");
    mewassert("bind world", gscript_bind_world(alloc, script, world));
    behaviors = gbeh_new(alloc, script, world, vfs);
    entity = gecs_Entity_new(alloc, world);
    mewassert(
        "attach behavior",
        behaviors && entity && gbeh_attach(alloc, behaviors, entity, "/glint-behavior-failure.lua")
    );
    state.fail_next = true;
    mewassert("snapshot allocation reports failure", !gbeh_update(alloc, behaviors));
    lua_pop(gscript_state(script), 1);
    state.fail_all = true;
    mewassert("persistent allocator failure is contained", !gbeh_update(alloc, behaviors));
    state.fail_all = false;
    lua_pop(gscript_state(script), 1);
    mewassert("retry succeeds", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(updates==1)");
    gbeh_destroy(alloc, behaviors);
    gscript_destroy(alloc, script);
    gvfs_destroy(alloc, vfs);
    gecs_World_free(alloc, world);
    mewassert("remove fixture", remove(file) == 0);
})

TEST(native_destroy_hooks, {
    const char *file = "glint-native-destroy.lua";
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gscript_Script *script = gscript_new(alloc);
    gvfs_Vfs *vfs = gvfs_create(alloc);
    mewassert("environment", world && script && vfs && gvfs_mount(alloc, vfs, "/", "."));
    gbeh__test_write_file(file, "return {setup=function(e) e.marker=42 end, destroy=function(e) "
                     "assert(e:valid() and e.marker==42); destroyed=(destroyed or 0)+1; e:destroy(); "
                     "assert(e:valid()); if fail_destroy then error('expected') end end}");
    mewassert("bind", gscript_bind_world(alloc, script, world));
    gbeh_Behaviors *behaviors = gbeh_new(alloc, script, world, vfs);
    mewassert("manager", behaviors);
    mewassert("single observer", !gbeh_new(alloc, script, world, vfs));
    gecs_Entity first = gecs_Entity_new(alloc, world);
    mewassert("attach", gbeh_attach(alloc, behaviors, first, "/glint-native-destroy.lua"));
    gecs_Entity_destroy(world, first);
    mewassert("native destroy", !gecs_Entity_alive(world, first));
    gbeh__test_run(alloc, script, "assert(destroyed==1)");
    mewassert("update", gbeh_update(alloc, behaviors));
    gbeh__test_run(alloc, script, "assert(destroyed==1); fail_destroy=true");
    gecs_Entity second = gecs_Entity_new(alloc, world);
    mewassert("attach error case", gbeh_attach(alloc, behaviors, second, "/glint-native-destroy.lua"));
    int top = lua_gettop(gscript_state(script));
    gecs_World_clear(alloc, world);
    mewassert("error discarded", lua_gettop(gscript_state(script)) == top);
    gbeh__test_run(alloc, script, "assert(destroyed==2); fail_destroy=false");
    gbeh_destroy(alloc, behaviors);
    gbeh__test_run(alloc, script, "assert(destroyed==2)");
    gscript_destroy(alloc, script);
    gvfs_destroy(alloc, vfs);
    gecs_World_free(alloc, world);
    mewassert("remove fixture", remove(file) == 0);
})

mewtest_main();

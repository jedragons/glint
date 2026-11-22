#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>
#include <glint/modules/assets/module.h>
#include <glint/modules/script/behaviors.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/module.h>
#include <glint/modules/script/script.h>
#include <glint/modules/vfs/module.h>
#include <glint/unit/unit.h>
#include <glint/vfs/vfs.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>
#include <string.h>

typedef struct gscript_behavior__TestFailAllocator {
    galloc_Allocator backing;
    bool fail_next;
    bool fail_all;
} gscript_behavior__TestFailAllocator;

typedef struct gscript_behavior__TestReentry {
    gscript_behavior_Behaviors* behaviors;
    bool deferred;
} gscript_behavior__TestReentry;

typedef struct gscript_behavior__TestModuleReentry {
    gmod_Modules* modules;
    bool unload_deferred;
    bool partial_deferred;
    bool free_deferred;
} gscript_behavior__TestModuleReentry;

static bool gscript_behavior__test_write_file(const char* path, const char* contents);
static void* gscript_behavior__test_fail_alloc(void* data, size_t size);
static void gscript_behavior__test_fail_free(void* data, void* ptr);
static void* gscript_behavior__test_fail_calloc(void* data, size_t count, size_t size);
static void* gscript_behavior__test_fail_realloc(void* data, void* ptr, size_t old_size, size_t new_size);
static bool gscript_behavior__test_reentry_load(galloc_Allocator alloc, void* instance, const char* path, void* user);
static bool gscript_behavior__test_module_reentry_load(
    galloc_Allocator alloc,
    void* instance,
    const char* path,
    void* user
);

typedef struct gscript_behavior__TestContext {
    galloc_Allocator alloc;
    gscript_behavior__TestFailAllocator allocations;
    gecs_World* world;
    gecs_World* other_world;
    gscript_lua_Runtime* script;
    lua_State* lua;
    gvfs_Vfs* vfs;
    gasset_Cache* cache;
    gscript_behavior_Behaviors* behaviors;
    gmod_Modules* modules;
    bool module_owned;
    gecs_Entity entities[3];
    size_t entity_count;
    const char* files[3];
    gscript_behavior__TestReentry reentry;
    gscript_behavior__TestModuleReentry module_reentry;
} gscript_behavior__TestContext;

static bool gscript_behavior__test_create(gscript_behavior__TestContext* context, bool module_owned);
static void gscript_behavior__test_free(void* user);
static bool gscript_behavior__test_run(gscript_behavior__TestContext* context, const char* source);
static bool gscript_behavior__test_allocator_recovery(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_file(
    gscript_behavior__TestContext* context,
    size_t slot,
    const char* path,
    const char* source
);
static bool gscript_behavior__test_attach(gscript_behavior__TestContext* context, const char* path, const char* name);
static bool gscript_behavior__test_parent(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_replacement(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_mutation(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_native(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_shared(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_module(gscript_behavior__TestContext* context);
static bool gscript_behavior__test_callback_errors(gscript_behavior__TestContext* context);

GUNIT_TEST(spawned_behavior_lifecycle,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert("attach the parent behavior", gscript_behavior__test_parent(&context));

    const char* after_setup =
        "assert(parent.count == 1)\n"
        "assert(parent:valid())\n";
    gunit_assert("parent setup runs on attach", gscript_behavior__test_run(&context, after_setup));

    gunit_assert("first update spawns the child", gscript_behavior_update(context.alloc, context.behaviors));
    const char* after_spawn =
        "assert(parent.count == 2)\n"
        "assert(child.started)\n"
        "assert(child:valid())\n";
    gunit_assert("child runs setup and stays alive", gscript_behavior__test_run(&context, after_spawn));

    gunit_assert("second update runs the child", gscript_behavior_update(context.alloc, context.behaviors));
    const char* after_destroy =
        "assert(parent.count == 3)\n"
        "assert(not child:valid())\n"
        "assert(child_destroyed == 1)\n";
    gunit_assert("child destroys itself during update", gscript_behavior__test_run(&context, after_destroy));
)

GUNIT_TEST(behavior_replacement,
    gscript_behavior__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_behavior__test_free, &context);
        gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
        gunit_assert("attach and replace the parent behavior", gscript_behavior__test_replacement(&context));
    })

    GUNIT_SECTION("destroys the original behavior and runs replacement setup", {
        gunit_assert(
            "run original destroy and replacement setup",
            gscript_behavior__test_run(
                &context,
                "assert(parent_destroyed == 1)\n"
                "assert(parent.replaced)\n"
            )
        );
        gunit_assert("update replacement", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "update uses the replacement callback",
            gscript_behavior__test_run(&context, "assert(parent.version == 1)")
        );
    })

    GUNIT_SECTION("reloads the replacement without repeating setup", {
        gunit_assert(
            "write reloaded behavior with a failing setup",
            gscript_behavior__test_write_file(
                context.files[2],
                "return {\n"
                "    setup = function(entity) error('reload must not call setup') end,\n"
                "    update = function(entity) entity.version = 200 end,\n"
                "    destroy = function(entity) reload_destroyed = true end\n"
                "}\n"
            )
        );
        gunit_assert("reload replacement", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "update uses the reloaded callback",
            gscript_behavior__test_run(&context, "assert(parent.version == 200)")
        );
    })

    GUNIT_SECTION("calls the latest destroy hook at shutdown", {
        gunit_assert(
            "write replacement destroy hook",
            gscript_behavior__test_write_file(
                context.files[2],
                "return {\n"
                "    update = function(entity) entity.version = 200 end,\n"
                "    destroy = function(entity) reload_destroyed = true end\n"
                "}\n"
            )
        );
        gunit_assert("reload replacement", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert("destroy manager", gscript_behavior_destroy(context.alloc, context.behaviors));
        context.behaviors = NULL;
        gasset_Cache_destroy(context.alloc, context.cache);
        context.cache = NULL;
        gunit_assert(
            "shutdown runs the reloaded destroy hook",
            gscript_behavior__test_run(&context, "assert(reload_destroyed)")
        );
    })
)

GUNIT_TEST(replacing_another_behavior_defers_its_update,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert("attach the replacing and original behaviors", gscript_behavior__test_mutation(&context));
    gunit_assert("update replaces another behavior", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "replacement runs setup and waits to update",
        gscript_behavior__test_run(
            &context,
            "assert(old_destroyed)\n"
            "assert(new_setup)\n"
            "assert(not new_updates)\n"
        )
    );
    gunit_assert("next update", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "replacement updates on the next frame",
        gscript_behavior__test_run(&context, "assert(new_updates == 1)")
    );
)

GUNIT_TEST(invalid_reload_preserves_script_and_can_be_retried,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert("attach the replacing and original behaviors", gscript_behavior__test_mutation(&context));
    gunit_assert("replace behavior", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("first replacement update", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "write malformed Lua",
        gscript_behavior__test_write_file(context.files[2], "return { update = function( invalid lua syntax")
    );
    gunit_assert("update succeeds after a bad reload", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "original update runs after a bad reload",
        gscript_behavior__test_run(&context, "assert(new_updates == 2)")
    );
    gunit_assert(
        "write a corrected update callback",
        gscript_behavior__test_write_file(
            context.files[2],
            "return {update=function(e) new_updates=(new_updates or 0)+10 end}"
        )
    );
    gunit_assert("fixed reload succeeds", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "corrected update adds ten",
        gscript_behavior__test_run(&context, "assert(new_updates == 12)")
    );
)

GUNIT_TEST(shutdown_rejects_new_behavior_spawns,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "write the spawn target",
        gscript_behavior__test_file(&context, 0, "glint-shutdown-child.lua", "return {}")
    );
    gunit_assert(
        "write a destroy callback that attempts to spawn",
        gscript_behavior__test_file(
            &context,
            1,
            "glint-shutdown-parent.lua",
            "return {\n"
            "    destroy = function()\n"
            "        shutdown_blocked = not pcall(function() spawn('/glint-shutdown-child.lua') end)\n"
            "    end\n"
            "}\n"
        )
    );
    gunit_assert(
        "attach parent behavior",
        gscript_behavior__test_attach(&context, "/glint-shutdown-parent.lua", "parent")
    );
    gunit_assert("destroy manager", gscript_behavior_destroy(context.alloc, context.behaviors));
    context.behaviors = NULL;
    gasset_Cache_destroy(context.alloc, context.cache);
    context.cache = NULL;
    gunit_assert(
        "destroy hook could not spawn during shutdown",
        gscript_behavior__test_run(&context, "assert(shutdown_blocked)")
    );
)

GUNIT_TEST(behavior_update_allocator_recovery,
    gscript_behavior__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_behavior__test_free, &context);
        gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
        gunit_assert(
            "write an update counter script",
            gscript_behavior__test_file(
                &context,
                0,
                "glint-behavior-failure.lua",
                "return {update=function(e) updates=(updates or 0)+1 end}"
            )
        );
        const char* counter_path = "/glint-behavior-failure.lua";
        gunit_assert("attach the counter behavior", gscript_behavior__test_attach(&context, counter_path, "e"));
    })

    GUNIT_SECTION("recovers after one allocation denial", {
        context.allocations.fail_next = true;
        gunit_assert("update recovers", gscript_behavior__test_allocator_recovery(&context));
    })

    GUNIT_SECTION("recovers after persistent allocation denial", {
        context.allocations.fail_all = true;
        gunit_assert("update recovers", gscript_behavior__test_allocator_recovery(&context));
    })
)

GUNIT_TEST(behavior_manager_allows_only_one_observer,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "single observer",
        gscript_behavior_new(context.script, context.world, context.vfs, context.cache) == NULL
    );
)

GUNIT_TEST(behavior_attach_rejects_another_world,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "write a behavior for the foreign entity",
        gscript_behavior__test_file(&context, 0, "glint-native-destroy.lua", "return {}")
    );
    context.other_world = gecs_World_new(context.alloc);
    gunit_assert("other world", context.other_world != NULL);
    gecs_Entity other = gecs_spawn(context.other_world);
    gunit_assert(
        "reject another world",
        !gscript_behavior_attach(context.behaviors, other, "/glint-native-destroy.lua")
    );
)

GUNIT_TEST(native_behavior_destruction,
    gscript_behavior__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_behavior__test_free, &context);
        gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
        gunit_assert("attach behavior with a native destroy callback", gscript_behavior__test_native(&context));
    })

    GUNIT_SECTION("calls the destroy hook once when the native entity is destroyed", {
        gecs_Entity_destroy(context.entities[0]);
        gunit_assert("native destroy removes entity", !gecs_Entity_alive(context.entities[0]));
        gunit_assert(
            "destroy callback ran once",
            gscript_behavior__test_run(&context, "assert(destroyed == 1)")
        );
        gunit_assert("update after native destroy", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "later update does not repeat destruction",
            gscript_behavior__test_run(&context, "assert(destroyed == 1)")
        );
    })

    GUNIT_SECTION("consumes the destroy hook error when clearing the world", {
        gunit_assert(
            "make the destroy callback fail",
            gscript_behavior__test_run(&context, "fail_destroy = true")
        );
        int top = lua_gettop(context.lua);
        gecs_World_clear(context.world);
        gunit_assert("destroy error discarded", lua_gettop(context.lua) == top);
        gunit_assert(
            "clearing the world runs the callback once",
            gscript_behavior__test_run(&context, "assert(destroyed == 1)")
        );
        gunit_assert("destroy manager after clear", gscript_behavior_destroy(context.alloc, context.behaviors));
        context.behaviors = NULL;
        gunit_assert(
            "manager destruction does not repeat the callback",
            gscript_behavior__test_run(&context, "assert(destroyed == 1)")
        );
    })
)

GUNIT_TEST(shared_behavior_asset,
    gscript_behavior__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_behavior__test_free, &context);
        gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
        gunit_assert("attach the same behavior asset to both entities", gscript_behavior__test_shared(&context));
    })

    GUNIT_SECTION("shares the asset and gives each component its own state", {
        gecs_Component type = gecs_Component_find(context.world, "gscript_behavior_Behavior");
        gunit_assert("component registered", gecs_Component_count(context.world, type) == 2);
        const gscript_behavior_Behavior* first = gecs_Component_get(context.entities[0], type);
        const gscript_behavior_Behavior* second = gecs_Component_get(context.entities[1], type);
        gunit_assert("first behavior is attached", first != NULL);
        gunit_assert("second behavior is attached", second != NULL);
        gunit_assert("both components refer to the same asset", first->asset == second->asset);

        const char* asset_path = gscript_asset_path(first->asset);
        gunit_assert("source asset has a path", asset_path != NULL);
        gunit_assert("asset path is the attached source", strcmp(asset_path, "/glint-shared-behavior.lua") == 0);
        gunit_assert("first component uses the supplied Lua runtime", first->lua == context.lua);
        gunit_assert("second component uses the supplied Lua runtime", second->lua == context.lua);
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, second->state_ref);
        gunit_assert("components own separate Lua tables", !lua_rawequal(context.lua, -1, -2));
        lua_pop(context.lua, 2);
    })

    GUNIT_SECTION("evaluates the source once for both entities", {
        gunit_assert("update both", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "single asset execution shares a counter across entities",
            gscript_behavior__test_run(
                &context,
                "assert(loads == 1)\n"
                "assert(a.count == 1)\n"
                "assert(b.count == 1)\n"
                "assert(math.min(a.shared, b.shared) == 1)\n"
                "assert(math.max(a.shared, b.shared) == 2)\n"
            )
        );
    })

    GUNIT_SECTION("preserves the state tables when reloaded", {
        gecs_Component type = gecs_Component_find(context.world, "gscript_behavior_Behavior");
        const gscript_behavior_Behavior* first = gecs_Component_get(context.entities[0], type);
        gunit_assert("first component", first != NULL);
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
        lua_setglobal(context.lua, "old_state");
        gunit_assert("update both", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "rewrite the shared update callback",
            gscript_behavior__test_write_file(
                context.files[0],
                "loads=loads+1; return {update=function(e) e.count=e.count+10 end}"
            )
        );
        gunit_assert("reload once", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "both entities update using the reloaded script",
            gscript_behavior__test_run(
                &context,
                "assert(loads == 2)\n"
                "assert(a.count == 11)\n"
                "assert(b.count == 11)\n"
            )
        );
        first = gecs_Component_get(context.entities[0], type);
        gunit_assert("component remains attached after reload", first != NULL);
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
        lua_setglobal(context.lua, "new_state");
        gunit_assert(
            "reload preserves the original state table",
            gscript_behavior__test_run(&context, "assert(rawequal(old_state, new_state))")
        );
    })

    GUNIT_SECTION("releases the old state when reattached to one entity", {
        gunit_assert("update both", gscript_behavior_update(context.alloc, context.behaviors));
        gecs_Component type = gecs_Component_find(context.world, "gscript_behavior_Behavior");
        const gscript_behavior_Behavior* first = gecs_Component_get(context.entities[0], type);
        gunit_assert("first component", first != NULL);
        luaL_requiref(context.lua, LUA_COLIBNAME, luaopen_coroutine, 1);
        lua_pop(context.lua, 1);
        gunit_assert(
            "read behavior state from a coroutine and create a weak table",
            gscript_behavior__test_run(
                &context,
                "weak_state = setmetatable({}, {__mode = 'v'})\n"
                "local reader = coroutine.create(function() assert(a.count == 1) end)\n"
                "assert(coroutine.resume(reader))\n"
            )
        );
        lua_getglobal(context.lua, "weak_state");
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
        lua_rawseti(context.lua, -2, 1);
        lua_pop(context.lua, 1);
        gunit_assert(
            "write the new update callback",
            gscript_behavior__test_write_file(
                context.files[0],
                "return {update = function(entity) entity.count = (entity.count or 0) + 10 end}"
            )
        );
        gunit_assert("reload shared source", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "replace behavior",
            gscript_behavior_attach(context.behaviors, context.entities[0], "/glint-shared-behavior.lua")
        );
        gunit_assert(
            "replaced state is collected and the other entity keeps its state",
            gscript_behavior__test_run(
                &context,
                "collectgarbage()\n"
                "collectgarbage()\n"
                "assert(weak_state[1] == nil)\n"
                "assert(a.count == nil)\n"
                "assert(a.shared == nil)\n"
                "assert(b.count == 11)\n"
            )
        );
        first = gecs_Component_get(context.entities[0], type);
        const gscript_behavior_Behavior* second = gecs_Component_get(context.entities[1], type);
        gunit_assert("replacement component exists", first != NULL);
        gunit_assert("second component remains attached", second != NULL);
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
        lua_rawgeti(context.lua, LUA_REGISTRYINDEX, second->state_ref);
        gunit_assert("replacement owns a separate Lua table", !lua_rawequal(context.lua, -1, -2));
        lua_pop(context.lua, 2);
    })
)

GUNIT_TEST(script_module_imports_owned_services,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("load script module and dependencies", gscript_behavior__test_create(&context, true));
    gvfsmod_Service* vfs = gecs_context_get(context.world, GECS_CONTEXT(context.world, gvfsmod_Service));
    gasset_Service* assets = gecs_context_get(context.world, GECS_CONTEXT(context.world, gasset_Service));
    gscript_Service* script = gecs_context_get(context.world, GECS_CONTEXT(context.world, gscript_Service));
    gunit_assert("VFS service published", vfs != NULL);
    gunit_assert("asset service published", assets != NULL);
    gunit_assert("script service published", script != NULL);
    gunit_assert("script module owns its service", script->module_owned);
    gunit_assert("asset module owns polling", !assets->host_polls);
)

GUNIT_TEST(script_module_behavior_updates,
    gscript_behavior__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_behavior__test_free, &context);
        gunit_assert("load script module and dependencies", gscript_behavior__test_create(&context, true));
        gunit_assert("attach behavior to the module entity", gscript_behavior__test_module(&context));
    })

    GUNIT_SECTION("updates attached components", {
        gunit_assert("update succeeded", gmod_Modules_update(context.alloc, context.modules));
        gunit_assert(
            "module update advances the behavior",
            gscript_behavior__test_run(&context, "assert(module_entity.count == 1)")
        );
    })

    GUNIT_SECTION("consumes a reload error and recovers after a correction", {
        gunit_assert("first update", gmod_Modules_update(context.alloc, context.modules));
        gunit_assert(
            "write malformed behavior source",
            gscript_behavior__test_write_file(context.files[0], "return { update=function( invalid lua syntax")
        );
        gunit_assert("bad reload retains previous script", gmod_Modules_update(context.alloc, context.modules));
        gunit_assert("reload error consumed", lua_gettop(context.lua) == 0);
        gunit_assert(
            "write a corrected update callback",
            gscript_behavior__test_write_file(context.files[0], "return {update=function(e) e.count=e.count+10 end}")
        );
        gunit_assert("reload recovers", gmod_Modules_update(context.alloc, context.modules));
        gunit_assert(
            "original and corrected update callbacks both ran",
            gscript_behavior__test_run(&context, "assert(module_entity.count == 12)")
        );
    })

    GUNIT_SECTION("retains an allocation diagnostic until the next successful update", {
        context.allocations.fail_all = true;
        bool updated = gmod_Modules_update(context.alloc, context.modules);
        context.allocations.fail_all = false;
        if (!updated) {
            gunit_assert("manager retains failure diagnostic", gmod_Modules_error(context.modules)[0] != '\0');
            gunit_assert("module consumes Lua error", lua_gettop(context.lua) == 0);
        }
        gunit_assert(
            "record the last successful update count",
            gscript_behavior__test_run(&context, "before_recovery = module_entity.count or 0")
        );
        gunit_assert("next update succeeds after recovery", gmod_Modules_update(context.alloc, context.modules));
        gunit_assert("successful update clears diagnostic", gmod_Modules_error(context.modules)[0] == '\0');
        gunit_assert(
            "recovery increments the module entity count",
            gscript_behavior__test_run(&context, "assert(module_entity.count == before_recovery + 1)")
        );
    })
)

GUNIT_TEST(script_module_unload_removes_context,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("load script module and dependencies", gscript_behavior__test_create(&context, true));
    gunit_assert("unload modules", gmod_Modules_unload(context.alloc, context.modules));
    context.script = NULL;
    context.behaviors = NULL;
    context.vfs = NULL;
    context.cache = NULL;
    gunit_assert(
        "script context removed",
        gecs_context_get(context.world, GECS_CONTEXT(context.world, gscript_Service)) == NULL
    );
)

GUNIT_TEST(behavior_destroy_is_deferred_during_cache_callback,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "write behavior used after deferred destruction",
        gscript_behavior__test_file(
            &context,
            0,
            "glint-destroy-reentry.lua",
            "return {update=function(e) e.ran=true end}"
        )
    );
    gunit_assert("attach behavior", gscript_behavior__test_attach(&context, "/glint-destroy-reentry.lua", "e"));
    context.reentry.behaviors = context.behaviors;
    gunit_assert(
        "register callback",
        gasset_Cache_register(
            context.alloc,
            context.cache,
            "reentry",
            1,
            gscript_behavior__test_reentry_load,
            NULL,
            NULL,
            &context.reentry
        )
    );
    gunit_assert(
        "asset loads during a destroy attempt",
        gasset_Cache_load(context.alloc, context.cache, "reentry", "test") != NULL
    );
    gunit_assert("destroy is deferred during the callback", context.reentry.deferred);
    gunit_assert("manager remains usable", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("retry destroy", gscript_behavior_destroy(context.alloc, context.behaviors));
    context.behaviors = NULL;
)

GUNIT_TEST(module_unload_is_deferred_during_asset_callback,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("load script module and dependencies", gscript_behavior__test_create(&context, true));
    context.module_reentry.modules = context.modules;
    gunit_assert(
        "register callback",
        gasset_Cache_register(
            context.alloc,
            context.cache,
            "reentry",
            1,
            gscript_behavior__test_module_reentry_load,
            NULL,
            NULL,
            &context.module_reentry
        )
    );
    gunit_assert("callback runs", gasset_Cache_load(context.alloc, context.cache, "reentry", "test") != NULL);
    gunit_assert("full unload deferred", context.module_reentry.unload_deferred);
    gunit_assert("partial unload deferred", context.module_reentry.partial_deferred);
    gunit_assert("manager free deferred", context.module_reentry.free_deferred);
    gscript_Service* script = gecs_context_get(context.world, GECS_CONTEXT(context.world, gscript_Service));
    gunit_assert("service is still installed", script != NULL);
    gunit_assert("service retains the Lua runtime", script->script == context.script);
    gunit_assert("behavior manager still updates", gscript_behavior_update(context.alloc, script->behaviors));
    gunit_assert("retry frees all modules", gmod_Modules_free(context.alloc, context.modules));
    context.modules = NULL;
    context.script = NULL;
    context.behaviors = NULL;
    context.vfs = NULL;
    context.cache = NULL;
    gunit_assert(
        "context removed",
        gecs_context_get(context.world, GECS_CONTEXT(context.world, gscript_Service)) == NULL
    );
)

GUNIT_TEST(shared_behavior_callback_errors,
    gscript_behavior__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gscript_behavior__test_free, &context);
        gunit_assert("create behavior manager", gscript_behavior__test_create(&context, false));
        gunit_assert("attach behaviors for callback error handling", gscript_behavior__test_callback_errors(&context));
    })

    GUNIT_SECTION("disables the shared asset when setup fails", {
        gunit_assert("setup error consumed", lua_gettop(context.lua) == 0);
        gunit_assert("other scripts continue", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "only one failing setup runs and the other asset updates",
            gscript_behavior__test_run(
                &context,
                "assert(setups == 1)\n"
                "assert(other == 1)\n"
            )
        );
    })

    GUNIT_SECTION("stops calling the shared update hook after it fails", {
        gunit_assert(
            "write a failing update hook",
            gscript_behavior__test_write_file(
                context.files[0],
                "return {\n"
                "    update = function(entity)\n"
                "        attempts = (attempts or 0) + 1\n"
                "        error('update failed')\n"
                "    end,\n"
                "    destroy = function(entity) destroys = (destroys or 0) + 1 end\n"
                "}\n"
            )
        );
        gunit_assert("reload errors do not fail frame", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert("repeat update succeeds", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "failing hook runs once while the other asset updates",
            gscript_behavior__test_run(
                &context,
                "assert(attempts == 1)\n"
                "assert(other == 2)\n"
            )
        );
    })

    GUNIT_SECTION("reenables the shared asset after a successful reload", {
        gunit_assert("initial update", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "write a failing update hook",
            gscript_behavior__test_write_file(
                context.files[0],
                "return {\n"
                "    update = function(entity)\n"
                "        attempts = (attempts or 0) + 1\n"
                "        error('update failed')\n"
                "    end\n"
                "}\n"
            )
        );
        gunit_assert("load failing update hook", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert("disabled update skipped", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "write malformed Lua for the disabled asset",
            gscript_behavior__test_write_file(context.files[0], "return { update = function( invalid syntax")
        );
        gunit_assert("invalid reload keeps asset disabled", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "write an update hook that succeeds",
            gscript_behavior__test_write_file(
                context.files[0],
                "return {update=function(e) recovered=(recovered or 0)+1 end}"
            )
        );
        gunit_assert("fixed reload recovers asset", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "recovered asset updates both entities and the independent asset kept updating",
            gscript_behavior__test_run(
                &context,
                "assert(recovered == 2)\n"
                "assert(other == 5)\n"
            )
        );
    })

    GUNIT_SECTION("consumes the error from the failing shared destroy hook", {
        gunit_assert(
            "write a failing destroy hook",
            gscript_behavior__test_write_file(
                context.files[0],
                "return {\n"
                "    destroy = function(entity)\n"
                "        destroys = (destroys or 0) + 1\n"
                "        error('destroy failed')\n"
                "    end\n"
                "}\n"
            )
        );
        gunit_assert("reload destroy hooks", gscript_behavior_update(context.alloc, context.behaviors));
        gunit_assert(
            "only one failing destroy hook runs and both entities are destroyed",
            gscript_behavior__test_run(
                &context,
                "callback_a:destroy()\n"
                "callback_b:destroy()\n"
                "assert(destroys == 1)\n"
                "assert(not callback_a:valid())\n"
                "assert(not callback_b:valid())\n"
            )
        );
        gunit_assert("stack remains balanced", lua_gettop(context.lua) == 0);
    })
)

gunit_main();

static bool gscript_behavior__test_allocator_recovery(gscript_behavior__TestContext* context) {
    int top = lua_gettop(context->lua);
    bool updated = gscript_behavior_update(context->alloc, context->behaviors);
    context->allocations.fail_next = false;
    context->allocations.fail_all = false;
    if (!updated) {
        if (!gunit_check("infrastructure failure leaves a Lua error", lua_gettop(context->lua) > top)) {
            return false;
        }
        lua_settop(context->lua, top);
    }
    if (!gunit_check(
            "record the last update count",
            gscript_behavior__test_run(context, "before_recovery = updates or 0")
        )) {
        return false;
    }
    if (!gunit_check("update succeeds after recovery", gscript_behavior_update(context->alloc, context->behaviors))) {
        return false;
    }
    return gunit_check(
        "recovery increments the update count",
        gscript_behavior__test_run(context, "assert(updates == before_recovery + 1)")
    );
}

static bool gscript_behavior__test_create(gscript_behavior__TestContext* context, bool module_owned) {
    static const galloc_FunctionTable functions = {
        .alloc = gscript_behavior__test_fail_alloc,
        .free = gscript_behavior__test_fail_free,
        .calloc = gscript_behavior__test_fail_calloc,
        .realloc = gscript_behavior__test_fail_realloc
    };
    context->allocations.backing = galloc_malloc_allocator();
    context->alloc = (galloc_Allocator) {.data = &context->allocations, .ftable = &functions};
    context->module_owned = module_owned;
    context->world = gecs_World_new(context->alloc);
    if (!gunit_check("create world", context->world != NULL)) {
        return false;
    }
    if (module_owned) {
        context->modules = gmod_Modules_new(context->world);
        if (!gunit_check("create module manager", context->modules != NULL)) {
            return false;
        }
        if (!gunit_check(
                "import script and dependencies",
                gmod_Modules_import(context->alloc, context->modules, gscript_module())
            )) {
            return false;
        }
        if (!gunit_check("load script services", gmod_Modules_load(context->alloc, context->modules))) {
            return false;
        }
        gvfsmod_Service* vfs = gecs_context_get(context->world, GECS_CONTEXT(context->world, gvfsmod_Service));
        gasset_Service* assets = gecs_context_get(context->world, GECS_CONTEXT(context->world, gasset_Service));
        gscript_Service* script = gecs_context_get(context->world, GECS_CONTEXT(context->world, gscript_Service));
        if (!gunit_check("VFS service available", vfs != NULL)) {
            return false;
        }
        if (!gunit_check("asset service available", assets != NULL)) {
            return false;
        }
        if (!gunit_check("script service available", script != NULL)) {
            return false;
        }
        context->vfs = vfs->vfs;
        context->cache = assets->cache;
        context->script = script->script;
        context->behaviors = script->behaviors;
    } else {
        context->script = gscript_lua_new(context->alloc);
        if (!gunit_check("create Lua runtime", context->script != NULL)) {
            return false;
        }
        context->vfs = gvfs_create(context->alloc);
        if (!gunit_check("create VFS", context->vfs != NULL)) {
            return false;
        }
        context->cache = gasset_Cache_create(context->alloc);
        if (!gunit_check("create cache", context->cache != NULL)) {
            return false;
        }
        if (!gunit_check("bind world", gscript_bindings_bind_world(context->script, context->world))) {
            return false;
        }
        context->behaviors = gscript_behavior_new(context->script, context->world, context->vfs, context->cache);
        if (!gunit_check("create behavior manager", context->behaviors != NULL)) {
            return false;
        }
    }
    context->lua = gscript_lua_state(context->script);
    return gunit_check("mount fixture directory", gvfs_mount(context->alloc, context->vfs, "/", "."));
}

static void gscript_behavior__test_free(void* user) {
    gscript_behavior__TestContext* context = user;
    context->allocations.fail_all = false;
    context->allocations.fail_next = false;
    if (context->module_owned) {
        if (context->modules != NULL) {
            gmod_Modules_free(context->alloc, context->modules);
        }
    } else {
        if (context->behaviors != NULL) {
            gscript_behavior_destroy(context->alloc, context->behaviors);
        }
        if (context->cache != NULL) {
            gasset_Cache_destroy(context->alloc, context->cache);
        }
        if (context->script != NULL) {
            gscript_lua_destroy(context->alloc, context->script);
        }
        if (context->vfs != NULL) {
            gvfs_destroy(context->alloc, context->vfs);
        }
    }
    if (context->other_world != NULL) {
        gecs_World_free(context->other_world);
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
    }
    for (size_t i = 0; i < 3; i++) {
        if (context->files[i] != NULL) {
            remove(context->files[i]);
        }
    }
}

static bool gscript_behavior__test_file(
    gscript_behavior__TestContext* context,
    size_t slot,
    const char* path,
    const char* source
) {
    if (!gunit_check("fixture slot available", slot < 3)) {
        return false;
    }
    context->files[slot] = path;
    if (!gscript_behavior__test_write_file(path, source)) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_attach(gscript_behavior__TestContext* context, const char* path, const char* name) {
    if (!gunit_check("entity slot available", context->entity_count < 3)) {
        return false;
    }
    gecs_Entity entity = gecs_spawn(context->world);
    context->entities[context->entity_count] = entity;
    context->entity_count += 1;
    if (!gunit_check("spawn entity", entity.id != 0)) {
        return false;
    }
    if (!gunit_check("attach behavior", gscript_behavior_attach(context->behaviors, entity, path))) {
        return false;
    }
    if (name != NULL) {
        gscript_bindings_push_entity(context->lua, entity);
        lua_setglobal(context->lua, name);
    }
    return true;
}

static bool gscript_behavior__test_parent(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-behavior-parent-test.lua",
            "return {\n"
            "    setup = function(parent) parent.count = 1 end,\n"
            "    update = function(parent)\n"
            "        parent.count = parent.count + 1\n"
            "        if not child then child = spawn('/glint-behavior-child-test.lua') end\n"
            "    end,\n"
            "    destroy = function(parent) parent_destroyed = (parent_destroyed or 0) + 1 end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_file(
            context,
            1,
            "glint-behavior-child-test.lua",
            "return {\n"
            "    setup = function(child) child.started = true end,\n"
            "    update = function(child) child:destroy() end,\n"
            "    destroy = function(child) child_destroyed = (child_destroyed or 0) + 1 end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-behavior-parent-test.lua", "parent")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_replacement(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_parent(context)) {
        return false;
    }
    if (!gscript_behavior__test_file(
            context,
            2,
            "glint-behavior-replacement-test.lua",
            "return {\n"
            "    setup = function(entity) entity.replaced = true end,\n"
            "    update = function(entity) entity.version = 1 end,\n"
            "    destroy = function(entity) replacement_destroyed = true end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_run(context, "parent:attach_module('/glint-behavior-replacement-test.lua')")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_mutation(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-mutate-a.lua",
            "return {\n"
            "    update = function(entity)\n"
            "        if not changed then\n"
            "            changed = true\n"
            "            b:attach_module('/glint-mutate-c.lua')\n"
            "        end\n"
            "    end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_file(
            context,
            1,
            "glint-mutate-b.lua",
            "return {update=function(e) old_updated=true end, destroy=function(e) old_destroyed=true end}"
        )) {
        return false;
    }
    if (!gscript_behavior__test_file(
            context,
            2,
            "glint-mutate-c.lua",
            "return {setup=function(e) new_setup=true end, update=function(e) new_updates=(new_updates or 0)+1 end}"
        )) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-mutate-a.lua", "a")) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-mutate-b.lua", "b")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_native(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-native-destroy.lua",
            "return {\n"
            "    setup = function(entity) entity.marker = 42 end,\n"
            "    destroy = function(entity)\n"
            "        assert(entity:valid())\n"
            "        assert(entity.marker == 42)\n"
            "        destroyed = (destroyed or 0) + 1\n"
            "        entity:destroy()\n"
            "        assert(entity:valid())\n"
            "        if fail_destroy then error('expected') end\n"
            "    end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-native-destroy.lua", "e")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_shared(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-shared-behavior.lua",
            "loads = (loads or 0) + 1\n"
            "local shared_updates = 0\n"
            "return {\n"
            "    setup = function(entity) entity.count = 0 end,\n"
            "    update = function(entity)\n"
            "        shared_updates = shared_updates + 1\n"
            "        entity.count = entity.count + 1\n"
            "        entity.shared = shared_updates\n"
            "    end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-shared-behavior.lua", "a")) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-shared-behavior.lua", "b")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_module(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-module-behavior.lua",
            "return {update=function(e) e.count=(e.count or 0)+1 end}"
        )) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-module-behavior.lua", "module_entity")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_callback_errors(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-callback-errors.lua",
            "return {\n"
            "    setup = function(entity)\n"
            "        setups = (setups or 0) + 1\n"
            "        error('setup failed')\n"
            "    end,\n"
            "    update = function(entity) error('must not update') end\n"
            "}\n"
        )) {
        return false;
    }
    if (!gscript_behavior__test_file(
            context,
            1,
            "glint-callback-other.lua",
            "return {update=function(e) other=(other or 0)+1 end}"
        )) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-callback-errors.lua", "callback_a")) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-callback-errors.lua", "callback_b")) {
        return false;
    }
    if (!gscript_behavior__test_attach(context, "/glint-callback-other.lua", "callback_other")) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_write_file(const char* path, const char* contents) {
    FILE* file = fopen(path, "wb");
    if (!gunit_check("open script fixture", file != NULL)) {
        return false;
    }
    size_t written = fwrite(contents, 1, strlen(contents), file);
    int closed = fclose(file);
    if (!gunit_check("write script fixture", written == strlen(contents))) {
        return false;
    }
    return gunit_check("close script fixture", closed == 0);
}

static bool gscript_behavior__test_run(gscript_behavior__TestContext* context, const char* source) {
    if (gscript_lua_run(context->alloc, context->script, source, strlen(source), "behavior test")) {
        return true;
    }

    const char* message = lua_tostring(context->lua, -1);
    return gunit_check(message != NULL ? message : "Lua chunk failed", false);
}

static void* gscript_behavior__test_fail_alloc(void* data, size_t size) {
    gscript_behavior__TestFailAllocator* state = data;
    if (state->fail_next || state->fail_all) {
        state->fail_next = false;
        return NULL;
    }

    return galloc_alloc(state->backing, size);
}

static void gscript_behavior__test_fail_free(void* data, void* ptr) {
    gscript_behavior__TestFailAllocator* state = data;
    galloc_free(state->backing, ptr);
}

static void* gscript_behavior__test_fail_calloc(void* data, size_t count, size_t size) {
    gscript_behavior__TestFailAllocator* state = data;
    if (state->fail_next || state->fail_all) {
        state->fail_next = false;
        return NULL;
    }

    return galloc_calloc(state->backing, count, size);
}

static void* gscript_behavior__test_fail_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gscript_behavior__TestFailAllocator* state = data;
    if (state->fail_next || state->fail_all) {
        state->fail_next = false;
        return NULL;
    }

    return galloc_realloc(state->backing, ptr, old_size, new_size);
}

static bool gscript_behavior__test_reentry_load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    (void)instance;
    (void)path;
    gscript_behavior__TestReentry* test = user;
    test->deferred = !gscript_behavior_destroy(alloc, test->behaviors);
    return true;
}

static bool gscript_behavior__test_module_reentry_load(
    galloc_Allocator alloc,
    void* instance,
    const char* path,
    void* user
) {
    (void)instance;
    (void)path;
    gscript_behavior__TestModuleReentry* test = user;
    test->unload_deferred = !gmod_Modules_unload(alloc, test->modules);
    test->partial_deferred = !gmod_Modules_unload_except(alloc, test->modules, GASSET_MODULE_NAME);
    test->free_deferred = !gmod_Modules_free(alloc, test->modules);
    return true;
}

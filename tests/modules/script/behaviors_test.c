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

typedef struct gscript_behavior__test_Reentry {
    gscript_behavior_Behaviors* behaviors;
    bool deferred;
} gscript_behavior__test_Reentry;

typedef struct gscript_behavior__test_ModuleReentry {
    gmod_Modules* modules;
    bool unload_deferred;
    bool partial_deferred;
    bool free_deferred;
} gscript_behavior__test_ModuleReentry;

static bool gscript_behavior__test_write_file(const char* path, const char* contents);
static bool gscript_behavior__test_run(galloc_Allocator alloc, gscript_lua_Runtime* script, const char* source);
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
    gscript_behavior__test_Reentry reentry;
    gscript_behavior__test_ModuleReentry module_reentry;
} gscript_behavior__TestContext;

static bool gscript_behavior__test_create(gscript_behavior__TestContext* context, bool module_owned);
static void gscript_behavior__test_free(void* user);
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

GUNIT_TEST(behavior_setup_runs_on_attach,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_parent(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(parent.count == 1 and parent:valid())")
    );
)

GUNIT_TEST(spawned_behavior_waits_until_next_update,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_parent(&context));
    gunit_assert("update parent", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "assert(parent.count == 2 and child.started and child:valid())"
        )
    );
)

GUNIT_TEST(behavior_can_destroy_itself_during_update,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_parent(&context));
    gunit_assert("spawn child", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("update child", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "assert(parent.count == 3 and not child:valid() and child_destroyed == 1)"
        )
    );
)

GUNIT_TEST(replacement_destroys_prior_behavior_and_runs_setup,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_replacement(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(parent_destroyed == 1 and parent.replaced)")
    );
    gunit_assert("update replacement", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(parent.version == 1)")
    );
)

GUNIT_TEST(reload_updates_hooks_without_repeating_setup,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_replacement(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[2],
            "return {setup=function(e) error('reload must not call setup') end, update=function(e) e.version=200 end, destroy=function(e) reload_destroyed=true end}"
        )
    );
    gunit_assert("reload replacement", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(parent.version == 200)")
    );
)

GUNIT_TEST(manager_shutdown_uses_reloaded_destroy_hook,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_replacement(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[2],
            "return {update=function(e) e.version=200 end, destroy=function(e) reload_destroyed=true end}"
        )
    );
    gunit_assert("reload replacement", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("destroy manager", gscript_behavior_destroy(context.alloc, context.behaviors));
    context.behaviors = NULL;
    gasset_Cache_destroy(context.alloc, context.cache);
    context.cache = NULL;
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(reload_destroyed)")
    );
)

GUNIT_TEST(replacing_another_behavior_defers_its_update,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_mutation(&context));
    gunit_assert("update replaces another behavior", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "assert(old_destroyed and new_setup and not new_updates)"
        )
    );
    gunit_assert("next update", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(new_updates == 1)")
    );
)

GUNIT_TEST(invalid_reload_preserves_script_and_can_be_retried,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_mutation(&context));
    gunit_assert("replace behavior", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("first replacement update", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(context.files[2], "return { update = function( invalid lua syntax")
    );
    gunit_assert("bad reload retains prior script", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[2],
            "return {update=function(e) new_updates=(new_updates or 0)+10 end}"
        )
    );
    gunit_assert("fixed reload succeeds", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(new_updates == 12)")
    );
)

GUNIT_TEST(shutdown_rejects_new_behavior_spawns,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_mutation(&context));
    gunit_assert("destroy manager", gscript_behavior_destroy(context.alloc, context.behaviors));
    context.behaviors = NULL;
    gasset_Cache_destroy(context.alloc, context.cache);
    context.cache = NULL;
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(shutdown_blocked)")
    );
)

GUNIT_TEST(update_recovers_after_transient_allocator_denial,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_file(
            &context,
            0,
            "glint-behavior-failure.lua",
            "return {update=function(e) updates=(updates or 0)+1 end}"
        )
    );
    gunit_assert("test helper succeeds", gscript_behavior__test_attach(&context, "/glint-behavior-failure.lua", "e"));
    context.allocations.fail_next = true;
    int top = lua_gettop(context.lua);
    bool updated = gscript_behavior_update(context.alloc, context.behaviors);
    context.allocations.fail_next = false;
    context.allocations.fail_all = false;
    if (!updated) {
        gunit_assert("infrastructure failure leaves a Lua error", lua_gettop(context.lua) > top);
        lua_settop(context.lua, top);
    }
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "before_recovery = updates or 0")
    );
    gunit_assert("update succeeds after allocator recovery", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(updates == before_recovery + 1)")
    );
)

GUNIT_TEST(update_recovers_after_persistent_allocator_denial,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_file(
            &context,
            0,
            "glint-behavior-failure.lua",
            "return {update=function(e) updates=(updates or 0)+1 end}"
        )
    );
    gunit_assert("test helper succeeds", gscript_behavior__test_attach(&context, "/glint-behavior-failure.lua", "e"));
    context.allocations.fail_all = true;
    int top = lua_gettop(context.lua);
    bool updated = gscript_behavior_update(context.alloc, context.behaviors);
    context.allocations.fail_next = false;
    context.allocations.fail_all = false;
    if (!updated) {
        gunit_assert("infrastructure failure leaves a Lua error", lua_gettop(context.lua) > top);
        lua_settop(context.lua, top);
    }
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "before_recovery = updates or 0")
    );
    gunit_assert("update succeeds after allocator recovery", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(updates == before_recovery + 1)")
    );
)

GUNIT_TEST(behavior_manager_allows_only_one_observer,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "single observer",
        gscript_behavior_new(context.script, context.world, context.vfs, context.cache) == NULL
    );
)

GUNIT_TEST(behavior_attach_rejects_another_world,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
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

GUNIT_TEST(native_entity_destroy_invokes_hook_once,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_native(&context));
    gecs_Entity_destroy(context.entities[0]);
    gunit_assert("native destroy removes entity", !gecs_Entity_alive(context.entities[0]));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(destroyed == 1)")
    );
    gunit_assert("update after native destroy", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(destroyed == 1)")
    );
)

GUNIT_TEST(world_clear_discards_destroy_callback_error,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_native(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "fail_destroy = true")
    );
    int top = lua_gettop(context.lua);
    gecs_World_clear(context.world);
    gunit_assert("destroy error discarded", lua_gettop(context.lua) == top);
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(destroyed == 1)")
    );
    gunit_assert("destroy manager after clear", gscript_behavior_destroy(context.alloc, context.behaviors));
    context.behaviors = NULL;
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(destroyed == 1)")
    );
)

GUNIT_TEST(shared_behavior_components_share_asset_but_not_state,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_shared(&context));
    gecs_Component type = gecs_Component_find(context.world, "gscript_behavior_Behavior");
    gunit_assert("component registered", gecs_Component_count(context.world, type) == 2);
    const gscript_behavior_Behavior* first = gecs_Component_get(context.entities[0], type);
    const gscript_behavior_Behavior* second = gecs_Component_get(context.entities[1], type);
    gunit_assert("components exist", first != NULL && second != NULL);
    gunit_assert(
        "components share asset",
        first->asset == second->asset && strcmp(gscript_asset_path(first->asset), "/glint-shared-behavior.lua") == 0
    );
    gunit_assert("components use the supplied Lua state", first->lua == context.lua && second->lua == context.lua);
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, second->state_ref);
    gunit_assert("components own separate Lua tables", !lua_rawequal(context.lua, -1, -2));
    lua_pop(context.lua, 2);
)

GUNIT_TEST(shared_behavior_source_executes_once,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_shared(&context));
    gunit_assert("update both", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "assert(loads == 1 and a.count == 1 and b.count == 1 and math.min(a.shared, b.shared) == 1 and math.max(a.shared, b.shared) == 2)"
        )
    );
)

GUNIT_TEST(shared_behavior_reload_preserves_lua_state,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_shared(&context));
    gecs_Component type = gecs_Component_find(context.world, "gscript_behavior_Behavior");
    const gscript_behavior_Behavior* first = gecs_Component_get(context.entities[0], type);
    gunit_assert("first component", first != NULL);
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
    lua_setglobal(context.lua, "old_state");
    gunit_assert("update both", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[0],
            "loads=loads+1; return {update=function(e) e.count=e.count+10 end}"
        )
    );
    gunit_assert("reload once", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "assert(loads == 2 and a.count == 11 and b.count == 11)"
        )
    );
    first = gecs_Component_get(context.entities[0], type);
    gunit_assert("component remains attached after reload", first != NULL);
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
    lua_setglobal(context.lua, "new_state");
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(rawequal(old_state, new_state))")
    );
)

GUNIT_TEST(replacing_shared_behavior_collects_old_state,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_shared(&context));
    gunit_assert("update both", gscript_behavior_update(context.alloc, context.behaviors));
    gecs_Component type = gecs_Component_find(context.world, "gscript_behavior_Behavior");
    const gscript_behavior_Behavior* first = gecs_Component_get(context.entities[0], type);
    gunit_assert("first component", first != NULL);
    luaL_requiref(context.lua, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(context.lua, 1);
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "weak_state=setmetatable({}, {__mode='v'}); local co=coroutine.create(function() assert(a.count==1) end); assert(coroutine.resume(co))"
        )
    );
    lua_getglobal(context.lua, "weak_state");
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
    lua_rawseti(context.lua, -2, 1);
    lua_pop(context.lua, 1);
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(context.files[0], "return {update=function(e) e.count=(e.count or 0)+10 end}")
    );
    gunit_assert("reload shared source", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "replace behavior",
        gscript_behavior_attach(context.behaviors, context.entities[0], "/glint-shared-behavior.lua")
    );
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "collectgarbage(); collectgarbage(); assert(weak_state[1] == nil and a.count == nil and a.shared == nil and b.count == 11)"
        )
    );
    first = gecs_Component_get(context.entities[0], type);
    const gscript_behavior_Behavior* second = gecs_Component_get(context.entities[1], type);
    gunit_assert("replacement and retained components exist", first != NULL && second != NULL);
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, first->state_ref);
    lua_rawgeti(context.lua, LUA_REGISTRYINDEX, second->state_ref);
    gunit_assert("replacement owns a separate Lua table", !lua_rawequal(context.lua, -1, -2));
    lua_pop(context.lua, 2);
)

GUNIT_TEST(script_module_imports_owned_services,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, true));
    gvfsmod_Service* vfs = gecs_context_get(context.world, GECS_CONTEXT(context.world, gvfsmod_Service));
    gasset_Service* assets = gecs_context_get(context.world, GECS_CONTEXT(context.world, gasset_Service));
    gscript_Service* script = gecs_context_get(context.world, GECS_CONTEXT(context.world, gscript_Service));
    gunit_assert("dependencies provided", vfs != NULL && assets != NULL && script != NULL && script->module_owned);
    gunit_assert("asset module owns polling", !assets->host_polls);
)

GUNIT_TEST(script_module_updates_attached_behaviors,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, true));
    gunit_assert("test helper succeeds", gscript_behavior__test_module(&context));
    gunit_assert("update succeeded", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(module_entity.count == 1)")
    );
)

GUNIT_TEST(script_module_reload_recovers_without_leaking_lua_errors,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, true));
    gunit_assert("test helper succeeds", gscript_behavior__test_module(&context));
    gunit_assert("first update", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(context.files[0], "return { update=function( invalid lua syntax")
    );
    gunit_assert("bad reload retains previous script", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert("reload error consumed", lua_gettop(context.lua) == 0);
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(context.files[0], "return {update=function(e) e.count=e.count+10 end}")
    );
    gunit_assert("reload recovers", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(module_entity.count == 12)")
    );
)

GUNIT_TEST(script_module_allocation_failure_retains_diagnostic_then_retries,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, true));
    gunit_assert("test helper succeeds", gscript_behavior__test_module(&context));
    context.allocations.fail_all = true;
    bool updated = gmod_Modules_update(context.alloc, context.modules);
    context.allocations.fail_all = false;
    if (!updated) {
        gunit_assert("manager retains failure diagnostic", gmod_Modules_error(context.modules)[0] != '\0');
        gunit_assert("module consumes Lua error", lua_gettop(context.lua) == 0);
    }
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "before_recovery = module_entity.count or 0")
    );
    gunit_assert("next update succeeds after allocator recovery", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert("successful update clears diagnostic", gmod_Modules_error(context.modules)[0] == '\0');
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(module_entity.count == before_recovery + 1)")
    );
)

GUNIT_TEST(script_module_unload_removes_context,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, true));
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
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_file(
            &context,
            0,
            "glint-destroy-reentry.lua",
            "return {update=function(e) e.ran=true end}"
        )
    );
    gunit_assert("test helper succeeds", gscript_behavior__test_attach(&context, "/glint-destroy-reentry.lua", "e"));
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
        "callback defers destroy",
        gasset_Cache_load(context.alloc, context.cache, "reentry", "test") != NULL && context.reentry.deferred
    );
    gunit_assert("manager remains usable", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("retry destroy", gscript_behavior_destroy(context.alloc, context.behaviors));
    context.behaviors = NULL;
)

GUNIT_TEST(module_unload_is_deferred_during_asset_callback,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, true));
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
    gunit_assert(
        "all unloads deferred",
        context.module_reentry.unload_deferred && context.module_reentry.partial_deferred
            && context.module_reentry.free_deferred
    );
    gscript_Service* script = gecs_context_get(context.world, GECS_CONTEXT(context.world, gscript_Service));
    gunit_assert(
        "services remain live",
        script != NULL && script->script == context.script && gscript_behavior_update(context.alloc, script->behaviors)
    );
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

GUNIT_TEST(setup_callback_error_disables_only_shared_asset,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_callback_errors(&context));
    gunit_assert("setup error consumed", lua_gettop(context.lua) == 0);
    gunit_assert("other scripts continue", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(setups == 1 and other == 1)")
    );
)

GUNIT_TEST(update_callback_error_disables_shared_asset_once,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_callback_errors(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[0],
            "return {update=function(e) attempts=(attempts or 0)+1; error('update failed') end, destroy=function(e) destroys=(destroys or 0)+1 end}"
        )
    );
    gunit_assert("reload errors do not fail frame", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("repeat update succeeds", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(attempts == 1 and other == 2)")
    );
)

GUNIT_TEST(disabled_asset_recovers_after_valid_reload,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_callback_errors(&context));
    gunit_assert("initial update", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[0],
            "return {update=function(e) attempts=(attempts or 0)+1; error('update failed') end}"
        )
    );
    gunit_assert("load failing update hook", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert("disabled update skipped", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(context.files[0], "return { update = function( invalid syntax")
    );
    gunit_assert("invalid reload retains disabled script", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[0],
            "return {update=function(e) recovered=(recovered or 0)+1 end}"
        )
    );
    gunit_assert("fixed reload recovers asset", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(context.alloc, context.script, "assert(recovered == 2 and other == 5)")
    );
)

GUNIT_TEST(shared_destroy_callback_error_is_consumed_once,
    gscript_behavior__TestContext context = {0};
    gunit_defer(gscript_behavior__test_free, &context);
    gunit_assert("create test context", gscript_behavior__test_create(&context, false));
    gunit_assert("test helper succeeds", gscript_behavior__test_callback_errors(&context));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_write_file(
            context.files[0],
            "return {destroy=function(e) destroys=(destroys or 0)+1; error('destroy failed') end}"
        )
    );
    gunit_assert("reload destroy hooks", gscript_behavior_update(context.alloc, context.behaviors));
    gunit_assert(
        "test helper succeeds",
        gscript_behavior__test_run(
            context.alloc,
            context.script,
            "callback_a:destroy(); callback_b:destroy(); assert(destroys == 1 and not callback_a:valid() and not callback_b:valid())"
        )
    );
    gunit_assert("stack remains balanced", lua_gettop(context.lua) == 0);
)

gunit_main();

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
        if (!gunit_check("script dependencies available", vfs != NULL && assets != NULL && script != NULL)) {
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
    if (!gunit_check("mount fixture directory", gvfs_mount(context->alloc, context->vfs, "/", "."))) {
        return false;
    }
    return true;
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
            "return {setup=function(e) e.count=1 end, update=function(e) "
            "e.count=e.count+1; if not child then child=spawn('/glint-behavior-child-test.lua') end end, "
            "destroy=function(e) parent_destroyed=(parent_destroyed or 0)+1 end}"
        )) {
        return false;
    }
    if (!gscript_behavior__test_file(
            context,
            1,
            "glint-behavior-child-test.lua",
            "return {setup=function(e) e.started=true end, update=function(e) e:destroy() end, "
            "destroy=function(e) child_destroyed=(child_destroyed or 0)+1 end}"
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
            "return {setup=function(e) e.replaced=true end, update=function(e) "
            "e.version=1 end, destroy=function(e) replacement_destroyed=true end}"
        )) {
        return false;
    }
    if (!gscript_behavior__test_run(
            context->alloc,
            context->script,
            "parent:attach_module('/glint-behavior-replacement-test.lua')"
        )) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_mutation(gscript_behavior__TestContext* context) {
    if (!gscript_behavior__test_file(
            context,
            0,
            "glint-mutate-a.lua",
            "return {update=function(e) if not changed then changed=true; b:attach_module('/glint-mutate-c.lua') end end, destroy=function(e) shutdown_blocked=not pcall(function() spawn('/glint-mutate-c.lua') end) end}"
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
            "return {setup=function(e) e.marker=42 end, destroy=function(e) "
            "assert(e:valid() and e.marker==42); destroyed=(destroyed or 0)+1; e:destroy(); "
            "assert(e:valid()); if fail_destroy then error('expected') end end}"
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
            "loads=(loads or 0)+1; local n=0; return {setup=function(e) e.count=0 end, update=function(e) n=n+1; e.count=e.count+1; e.shared=n end}"
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
            "return {setup=function(e) setups=(setups or 0)+1; error('setup failed') end, update=function(e) error('must not update') end}"
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
    if (!gunit_check("close script fixture", closed == 0)) {
        return false;
    }
    return true;
}

static bool gscript_behavior__test_run(galloc_Allocator alloc, gscript_lua_Runtime* script, const char* source) {
    if (!gscript_lua_run(alloc, script, source, strlen(source), "behavior test")) {
        fprintf(stderr, "%s\n", lua_tostring(gscript_lua_state(script), -1));
        if (!gunit_check("Lua assertion chunk succeeds", false)) {
            return false;
        }
    }
    return true;
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
    gscript_behavior__test_Reentry* test = user;
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
    gscript_behavior__test_ModuleReentry* test = user;
    test->unload_deferred = !gmod_Modules_unload(alloc, test->modules);
    test->partial_deferred = !gmod_Modules_unload_except(alloc, test->modules, GASSET_MODULE_NAME);
    test->free_deferred = !gmod_Modules_free(alloc, test->modules);
    return true;
}

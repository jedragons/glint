#include <glint/modules/assets/cache.h>
#include <glint/modules/assets/module.h>
#include <glint/modules/vfs/module.h>
#include <glint/unit/unit.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// A fake asset type: an owned string plus a load counter, letting tests
// observe cache hits, reloads, failures and teardown order.
typedef struct gasset__TestFakeAsset {
    char* text;
    size_t generation;
} gasset__TestFakeAsset;

typedef struct gasset__TestReentrantContext {
    gasset_Cache* cache;
    bool trigger;
    bool fail;
    size_t callbacks;
    size_t unloads;
    bool* fail_realloc;
} gasset__TestReentrantContext;

typedef struct gasset__TestFailingAllocator {
    galloc_Allocator backing;
    bool fail_realloc;
    size_t denied;
} gasset__TestFailingAllocator;

typedef struct gasset__TestModuleState {
    gecs_World* world;
    gecs_Component vfs_type;
    size_t unloaded;
} gasset__TestModuleState;

typedef struct gasset__TestContext {
    galloc_Allocator alloc;
    gasset_Cache* cache;
    size_t loads;
    size_t reloads;
    size_t unloads;
    size_t load_attempts;
    const gasset__TestFakeAsset* unload_order[8];
    size_t unload_count;
    bool fail_loads;
    bool fail_reloads;
    gasset__TestReentrantContext reentrant;
    gasset__TestFailingAllocator allocations;
    gecs_World* world;
    gmod_Modules* modules;
    gvfsmod_Service host_vfs;
    gasset_Service host_assets;
    gasset__TestModuleState module_state;
} gasset__TestContext;

static bool gasset__test_create(gasset__TestContext* ctx);
static bool gasset__test_create_registered(gasset__TestContext* ctx, bool reload);
static bool gasset__test_create_reentrant(gasset__TestContext* ctx, bool reload, bool failing);
static bool gasset__test_create_modules(gasset__TestContext* ctx);
static void gasset__test_destroy(void* user);
static bool gasset__test_register(gasset__TestContext* ctx, const char* name, bool reload);
static bool gasset__test_watch_fixture(gasset__TestContext* ctx);

static bool gasset__test_fake_load(galloc_Allocator alloc, void* instance, const char* path, void* user);
static bool gasset__test_fake_reload(galloc_Allocator alloc, void* instance, const char* path, void* user);
static void gasset__test_fake_unload(galloc_Allocator alloc, void* instance, void* user);
static bool gasset__test_write_asset(const char* path, const char* data);
static bool gasset__test_reentrant_work(galloc_Allocator alloc, const char* path, gasset__TestReentrantContext* ctx);
static bool gasset__test_reentrant_load(galloc_Allocator alloc, void* instance, const char* path, void* user);
static bool gasset__test_reentrant_reload(galloc_Allocator alloc, void* instance, const char* path, void* user);
static void gasset__test_reentrant_unload(galloc_Allocator alloc, void* instance, void* user);
static void* gasset__test_failing_alloc(void* data, size_t size);
static void gasset__test_failing_free(void* data, void* ptr);
static void* gasset__test_failing_calloc(void* data, size_t count, size_t size);
static void* gasset__test_failing_realloc(void* data, void* ptr, size_t old_size, size_t new_size);
static bool gasset__test_module_load(galloc_Allocator alloc, void* instance, const char* path, void* user);
static void gasset__test_module_unload(galloc_Allocator alloc, void* instance, void* user);

GUNIT_TEST(asset_unregister_only_removes_selected_type, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create", gasset__test_create(&ctx));

    gunit_assert("first type registered", gasset__test_register(&ctx, "first", true));
    gunit_assert("second type registered", gasset__test_register(&ctx, "second", true));
    void* first = gasset_Cache_load(ctx.alloc, ctx.cache, "first", "/one");
    void* second = gasset_Cache_load(ctx.alloc, ctx.cache, "second", "/two");
    gunit_assert("both loaded", first != NULL && second != NULL);
    gunit_assert("type removed", gasset_Cache_unregister(ctx.alloc, ctx.cache, "first"));
    gunit_assert("only removed type unloaded", ctx.unloads == 1 && ctx.unload_order[0] == first);
    gunit_assert("other type remains accessible", gasset_Cache_get(ctx.cache, "second", "/two") == second);
    gunit_return();
})

GUNIT_TEST(asset_unregister_name_can_be_reused, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gunit_assert("type removed", gasset_Cache_unregister(ctx.alloc, ctx.cache, "fake"));
    gunit_assert("removed name is reusable", gasset__test_register(&ctx, "fake", false));
    gunit_return();
})

GUNIT_TEST(asset_registration_rejects_duplicate, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gunit_assert("duplicate type rejected", !gasset__test_register(&ctx, "fake", true));
    gunit_return();
})

GUNIT_TEST(asset_registration_rejects_empty_name, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create", gasset__test_create(&ctx));

    gunit_assert("empty type rejected", !gasset__test_register(&ctx, "", true));
    gunit_return();
})

GUNIT_TEST(asset_registration_requires_loader, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create", gasset__test_create(&ctx));

    gunit_assert(
        "missing load rejected",
        !gasset_Cache_register(
            ctx.alloc,
            ctx.cache,
            "other",
            sizeof(gasset__TestFakeAsset),
            NULL,
            gasset__test_fake_reload,
            gasset__test_fake_unload,
            &ctx
        )
    );
    gunit_return();
})

GUNIT_TEST(asset_registration_requires_instance_size, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create", gasset__test_create(&ctx));

    gunit_assert(
        "zero instance size rejected",
        !gasset_Cache_register(
            ctx.alloc,
            ctx.cache,
            "tiny",
            0,
            gasset__test_fake_load,
            gasset__test_fake_reload,
            gasset__test_fake_unload,
            &ctx
        )
    );
    gunit_return();
})

GUNIT_TEST(asset_unknown_type_does_not_load, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create", gasset__test_create(&ctx));

    gunit_assert("unknown type does not load", gasset_Cache_load(ctx.alloc, ctx.cache, "missing", "/a") == NULL);
    gunit_return();
})

GUNIT_TEST(asset_empty_cache_has_nothing_to_unload, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create", gasset__test_create(&ctx));

    gasset_Cache_destroy(ctx.alloc, ctx.cache);
    ctx.cache = NULL;
    gunit_assert("nothing to unload", ctx.unload_count == 0);
    gunit_return();
})

GUNIT_TEST(asset_load_initializes_instance, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gasset__TestFakeAsset* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua");
    gunit_assert(
        "asset contents loaded",
        asset != NULL && asset->text != NULL && strcmp(asset->text, "/assets/hero.lua") == 0
    );
    gunit_assert("load ran once", ctx.loads == 1);
    gunit_return();
})

GUNIT_TEST(asset_load_reuses_cached_instance, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    void* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua");
    gunit_assert("asset loaded", asset != NULL);
    gunit_assert(
        "cache hit returns same instance",
        gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua") == asset
    );
    gunit_assert("cache hit does not reload", ctx.loads == 1);
    gunit_return();
})

GUNIT_TEST(asset_distinct_paths_have_distinct_instances, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    void* first = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua");
    void* second = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/map.lua");
    gunit_assert(
        "different paths produce different instances",
        first != NULL && second != NULL && second != first && ctx.loads == 2
    );
    gunit_return();
})

GUNIT_TEST(asset_get_and_contains_only_find_loaded_paths, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    void* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua");
    gunit_assert("asset loaded", asset != NULL);
    gunit_assert("contains finds cached assets", gasset_Cache_contains(ctx.cache, "fake", "/assets/hero.lua"));
    gunit_assert("get returns cached instance", gasset_Cache_get(ctx.cache, "fake", "/assets/hero.lua") == asset);
    gunit_assert("contains misses uncached assets", !gasset_Cache_contains(ctx.cache, "fake", "/assets/nope.lua"));
    gunit_assert("get misses uncached assets", gasset_Cache_get(ctx.cache, "fake", "/assets/nope.lua") == NULL);
    gunit_return();
})

GUNIT_TEST(asset_destroy_unloads_in_reverse_load_order, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    void* first = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/one");
    void* second = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/two");
    gunit_assert("both assets loaded", first != NULL && second != NULL);
    gasset_Cache_destroy(ctx.alloc, ctx.cache);
    ctx.cache = NULL;
    gunit_assert("every asset unloaded", ctx.unloads == 2);
    gunit_assert("later asset unloaded first", ctx.unload_order[0] == second && ctx.unload_order[1] == first);
    gunit_return();
})

GUNIT_TEST(asset_failed_load_is_not_cached, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    ctx.fail_loads = true;
    gunit_assert("failed load returns nothing", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad") == NULL);
    gunit_assert("failed load is not cached", !gasset_Cache_contains(ctx.cache, "fake", "/bad"));
    gunit_return();
})

GUNIT_TEST(asset_failed_load_can_be_retried, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    ctx.fail_loads = true;
    gunit_assert("initial load fails", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad") == NULL);
    gunit_assert("failed load may be retried", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad") == NULL);
    gunit_assert("retries reach loader", ctx.load_attempts == 2);
    ctx.fail_loads = false;
    void* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad");
    gunit_assert("retry succeeds", asset != NULL && ctx.loads == 1 && ctx.load_attempts == 3);
    gasset_Cache_destroy(ctx.alloc, ctx.cache);
    ctx.cache = NULL;
    gunit_assert("only successful asset unloaded", ctx.unload_count == 1 && ctx.unload_order[0] == asset);
    gunit_return();
})

GUNIT_TEST(asset_reload_requires_loaded_asset, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gunit_assert("reload without load fails", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    gunit_return();
})

GUNIT_TEST(asset_reload_updates_instance_in_place, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gasset__TestFakeAsset* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
    gunit_assert("asset loaded", asset != NULL);
    gunit_assert("reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    gunit_assert("reload retains instance", gasset_Cache_get(ctx.cache, "fake", "/a") == asset);
    gunit_assert("reload updates contents", strcmp(asset->text, "/a") == 0 && asset->generation == 1);
    gunit_assert("reload ran once", ctx.reloads == 1 && ctx.loads == 1);
    gunit_return();
})

GUNIT_TEST(asset_failed_reload_preserves_contents, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gasset__TestFakeAsset* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
    gunit_assert("asset loaded", asset != NULL);
    ctx.fail_reloads = true;
    gunit_assert("failed reload reported", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    gunit_assert("old contents preserved", strcmp(asset->text, "/a") == 0 && asset->generation == 1);
    gunit_assert("failed reload stays cached", gasset_Cache_contains(ctx.cache, "fake", "/a"));
    gunit_return();
})

GUNIT_TEST(asset_watch_rejects_unknown_type, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));
    gunit_assert("watch fixture", gasset__test_watch_fixture(&ctx));

    gunit_assert(
        "unknown mapping rejected",
        !gasset_Cache_watch(ctx.alloc, ctx.cache, "missing", "/a", "glint-cache-test.lua")
    );
    gunit_return();
})

GUNIT_TEST(asset_watch_unchanged_file_does_not_reload, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));
    gunit_assert("watch fixture", gasset__test_watch_fixture(&ctx));

    gasset_Cache_poll(ctx.alloc, ctx.cache);
    gunit_assert("unchanged files do not reload", ctx.reloads == 0);
    gunit_return();
})

GUNIT_TEST(asset_watch_reload_only_touches_mapped_asset, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));
    gunit_assert("watch fixture", gasset__test_watch_fixture(&ctx));

    gunit_assert("write asset", gasset__test_write_asset("glint-cache-test.lua", "changed contents"));
    gasset_Cache_poll(ctx.alloc, ctx.cache);
    gasset__TestFakeAsset* first = gasset_Cache_get(ctx.cache, "fake", "/a");
    gasset__TestFakeAsset* second = gasset_Cache_get(ctx.cache, "fake", "/b");
    gunit_assert("changed file reloads once", ctx.reloads == 1);
    gunit_assert("mapped asset reloaded", first != NULL && strcmp(first->text, "/a") == 0 && first->generation == 1);
    gunit_assert("unmapped asset untouched", second != NULL && second->generation == 2 && ctx.loads == 2);
    gunit_return();
})

GUNIT_TEST(asset_watch_multiple_files_deduplicates_mappings, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));
    gunit_assert("watch fixture", gasset__test_watch_fixture(&ctx));

    gunit_assert(
        "second native file",
        gasset_Cache_watch(ctx.alloc, ctx.cache, "fake", "/a", "glint-cache-test-extra.lua")
    );
    gunit_assert(
        "duplicate watch accepted",
        gasset_Cache_watch(ctx.alloc, ctx.cache, "fake", "/a", "glint-cache-test-extra.lua")
    );
    gunit_assert("write asset", gasset__test_write_asset("glint-cache-test-extra.lua", "second changed contents"));
    gasset_Cache_poll(ctx.alloc, ctx.cache);
    gunit_assert("second native file reloads once", ctx.reloads == 1);
    gunit_return();
})

GUNIT_TEST(asset_unregister_removes_all_watch_mappings, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));
    gunit_assert("watch fixture", gasset__test_watch_fixture(&ctx));

    gunit_assert(
        "second native file",
        gasset_Cache_watch(ctx.alloc, ctx.cache, "fake", "/a", "glint-cache-test-extra.lua")
    );
    gunit_assert("type unregistered", gasset_Cache_unregister(ctx.alloc, ctx.cache, "fake"));
    gunit_assert("write asset", gasset__test_write_asset("glint-cache-test.lua", "changed after unregister"));
    gunit_assert("write asset", gasset__test_write_asset("glint-cache-test-extra.lua", "changed after unregister"));
    gasset_Cache_poll(ctx.alloc, ctx.cache);
    gunit_assert("unregister removed every mapping", ctx.reloads == 0 && ctx.unload_count == 2);
    gunit_return();
})

GUNIT_TEST(asset_fallback_reload_keeps_instance, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, false));

    void* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
    gunit_assert("asset loaded", asset != NULL);
    gunit_assert("fallback reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    gunit_assert("fallback unloads and loads", ctx.unloads == 1 && ctx.loads == 2);
    gunit_assert("instance stays in place", gasset_Cache_get(ctx.cache, "fake", "/a") == asset);
    gunit_return();
})

GUNIT_TEST(asset_failed_fallback_reload_evicts_stale_instance, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, false));

    gunit_assert("asset loaded", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a") != NULL);
    ctx.fail_loads = true;
    gunit_assert("failed fallback reload reported", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    gunit_assert("evicted asset is gone", !gasset_Cache_contains(ctx.cache, "fake", "/a"));
    gunit_assert("stale instance unloaded", ctx.unload_count == 1);
    gunit_return();
})

GUNIT_TEST(asset_evicted_fallback_path_can_load_again, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, false));

    gunit_assert("asset loaded", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a") != NULL);
    gunit_assert("initial reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    ctx.fail_loads = true;
    gunit_assert("failed reload evicts", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
    ctx.fail_loads = false;
    void* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
    gunit_assert("evicted path loads again", asset != NULL && ctx.loads == 3);
    gasset_Cache_destroy(ctx.alloc, ctx.cache);
    ctx.cache = NULL;
    gunit_assert("surviving instance unloaded last", ctx.unload_count == 3 && ctx.unload_order[2] == asset);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_load_retains_dependencies, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, false, false));

    ctx.reentrant.trigger = true;
    int* main = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    gunit_assert("reentrant initial load", main != NULL && *main == 42 && ctx.reentrant.callbacks == 1);
    gunit_assert("dependency retained", gasset_Cache_get(ctx.cache, "outer", "dep1-2") != NULL);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_fallback_reload_keeps_instance, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, false, false));

    ctx.reentrant.trigger = true;
    void* main = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    gunit_assert("initial load", main != NULL);
    gunit_assert("fallback reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
    gunit_assert("instance retained", gasset_Cache_get(ctx.cache, "outer", "main") == main);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_failed_reload_preserves_dependencies, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, false, false));

    ctx.reentrant.trigger = true;
    gunit_assert("initial load", gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main") != NULL);
    ctx.reentrant.fail = true;
    gunit_assert("fallback reload fails", !gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
    gunit_assert("failed entry evicted", gasset_Cache_get(ctx.cache, "outer", "main") == NULL);
    gunit_assert("dependencies survive", gasset_Cache_get(ctx.cache, "outer", "dep2-2") != NULL);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_failed_initial_load_can_retry, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, false, false));

    ctx.reentrant.trigger = true;
    ctx.reentrant.fail = true;
    gunit_assert("initial load fails", gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main") == NULL);
    ctx.reentrant.fail = false;
    gunit_assert("retry succeeds", gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main") != NULL);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_custom_reload_failure_retains_entry, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, true, false));

    int* main = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    gunit_assert("initial load", main != NULL);
    ctx.reentrant.fail = true;
    gunit_assert("custom reload fails", !gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
    gunit_assert("failure retains entry", gasset_Cache_get(ctx.cache, "outer", "main") == main);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_custom_reload_updates_instance, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, true, false));

    int* main = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    gunit_assert("initial load", main != NULL);
    gunit_assert("custom reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
    gunit_assert("custom reload updates instance", *main == 99);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_load_handles_optional_allocation_failure, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, false, true));

    ctx.reentrant.trigger = true;
    ctx.reentrant.fail_realloc = &ctx.allocations.fail_realloc;
    int* main = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    if (main == NULL) {
        gunit_assert("failure follows an allocation denial", ctx.allocations.denied > 0);
        gunit_assert("loaded instance cleaned up", ctx.reentrant.unloads == 1);
        gunit_assert("failed instance not cached", gasset_Cache_get(ctx.cache, "outer", "main") == NULL);
    } else {
        gunit_assert(
            "successful load retains its instance",
            *main == 42 && gasset_Cache_get(ctx.cache, "outer", "main") == main
        );
        gunit_assert("cached instance remains owned", ctx.reentrant.unloads == 0);
    }
    gunit_assert("reentrant dependency survives", gasset_Cache_get(ctx.cache, "outer", "dep1-2") != NULL);
    ctx.allocations.fail_realloc = false;
    gunit_assert("type can be unloaded", gasset_Cache_unregister(ctx.alloc, ctx.cache, "outer"));
    gunit_assert("each loaded instance unloaded once", ctx.reentrant.unloads == 4);
    gunit_return();
})

GUNIT_TEST(asset_reentrant_load_can_retry_after_optional_allocation_failure, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create reentrant", gasset__test_create_reentrant(&ctx, false, true));

    ctx.reentrant.trigger = true;
    ctx.reentrant.fail_realloc = &ctx.allocations.fail_realloc;
    void* main = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    if (main == NULL) {
        gunit_assert("failure follows an allocation denial", ctx.allocations.denied > 0);
        gunit_assert("failed instance cleaned up", ctx.reentrant.unloads == 1);
    }
    ctx.allocations.fail_realloc = false;
    ctx.reentrant.fail_realloc = NULL;
    ctx.reentrant.trigger = false;
    void* retried = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    gunit_assert("path remains loadable", retried != NULL);
    gunit_assert("successful prior load remains cached", main == NULL || retried == main);
    gunit_return();
})

GUNIT_TEST(asset_module_creates_owned_services, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create modules", gasset__test_create_modules(&ctx));

    gmod_Module assets = gasset_module();
    gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
    gunit_assert("load dependencies", gmod_Modules_load(ctx.alloc, ctx.modules));
    gvfsmod_Service* vfs = gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gvfsmod_Service));
    gasset_Service* asset = gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gasset_Service));
    gunit_assert("owned VFS", vfs != NULL && vfs->vfs != NULL && vfs->module_owned);
    gunit_assert("owned cache", asset != NULL && asset->cache != NULL && asset->module_owned);
    gunit_return();
})

GUNIT_TEST(asset_module_unloads_before_vfs, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create modules", gasset__test_create_modules(&ctx));

    gmod_Module assets = gasset_module();
    gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
    gunit_assert("load dependencies", gmod_Modules_load(ctx.alloc, ctx.modules));
    gecs_Component vt = GECS_CONTEXT(ctx.world, gvfsmod_Service);
    gecs_Component at = GECS_CONTEXT(ctx.world, gasset_Service);
    gasset_Service* service = gecs_context_get(ctx.world, at);
    gunit_assert("asset service exists", service != NULL);
    ctx.module_state = (gasset__TestModuleState) {.world = ctx.world, .vfs_type = vt};
    gunit_assert(
        "register lifetime observer",
        gasset_Cache_register(
            ctx.alloc,
            service->cache,
            "module-test",
            sizeof(int),
            gasset__test_module_load,
            NULL,
            gasset__test_module_unload,
            &ctx.module_state
        )
    );
    gunit_assert("load observed asset", gasset_Cache_load(ctx.alloc, service->cache, "module-test", "item") != NULL);
    gmod_Modules_update(ctx.alloc, ctx.modules);
    gunit_assert("unload dependencies", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert("asset unloaded while VFS alive", ctx.module_state.unloaded == 1);
    gunit_assert(
        "owned services removed",
        gecs_context_get(ctx.world, vt) == NULL && gecs_context_get(ctx.world, at) == NULL
    );
    gunit_return();
})

GUNIT_TEST(asset_module_preserves_borrowed_services, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create modules", gasset__test_create_modules(&ctx));

    ctx.host_vfs.vfs = gvfs_create(ctx.alloc);
    ctx.host_assets.cache = ctx.cache;
    gecs_Component vt = GECS_CONTEXT(ctx.world, gvfsmod_Service);
    gecs_Component at = GECS_CONTEXT(ctx.world, gasset_Service);
    gunit_assert("provide VFS", ctx.host_vfs.vfs != NULL && gecs_context_set(ctx.world, vt, &ctx.host_vfs) != NULL);
    gunit_assert("provide cache", gecs_context_set(ctx.world, at, &ctx.host_assets) != NULL);
    gmod_Module assets = gasset_module();
    gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
    gunit_assert("borrow services", gmod_Modules_load(ctx.alloc, ctx.modules));
    gunit_assert("unload module", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert(
        "borrowed VFS preserved",
        ((gvfsmod_Service*)gecs_context_get(ctx.world, vt))->vfs == ctx.host_vfs.vfs
    );
    gunit_assert("borrowed cache preserved", ((gasset_Service*)gecs_context_get(ctx.world, at))->cache == ctx.cache);
    gunit_return();
})

GUNIT_TEST(asset_module_invalid_host_rolls_back_dependency, {
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create modules", gasset__test_create_modules(&ctx));

    gecs_Component at = GECS_CONTEXT(ctx.world, gasset_Service);
    gunit_assert("set invalid cache", gecs_context_set(ctx.world, at, &ctx.host_assets) != NULL);
    gmod_Module assets = gasset_module();
    gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
    gunit_assert("invalid host cache fails", !gmod_Modules_load(ctx.alloc, ctx.modules));
    gunit_assert(
        "dependency rolled back",
        gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gvfsmod_Service)) == NULL
    );
    gunit_return();
})

gunit_main();

static bool gasset__test_create(gasset__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    ctx->cache = gasset_Cache_create(ctx->alloc);
    if (!gunit_check("cache created", ctx->cache != NULL)) {
        return false;
    }

    return true;
}

static bool gasset__test_register(gasset__TestContext* ctx, const char* name, bool reload) {
    return gasset_Cache_register(
        ctx->alloc,
        ctx->cache,
        name,
        sizeof(gasset__TestFakeAsset),
        gasset__test_fake_load,
        reload ? gasset__test_fake_reload : NULL,
        gasset__test_fake_unload,
        ctx
    );
}

static bool gasset__test_create_registered(gasset__TestContext* ctx, bool reload) {
    if (!gasset__test_create(ctx)) {
        return false;
    }
    if (!gunit_check("register fake asset type", gasset__test_register(ctx, "fake", reload))) {
        return false;
    }

    return true;
}

static bool gasset__test_create_reentrant(gasset__TestContext* ctx, bool reload, bool failing) {
    static const galloc_FunctionTable table = {
        gasset__test_failing_alloc,
        gasset__test_failing_free,
        gasset__test_failing_calloc,
        gasset__test_failing_realloc
    };
    ctx->allocations.backing = galloc_malloc_allocator();
    ctx->alloc = failing ? (galloc_Allocator) {.data = &ctx->allocations, .ftable = &table} : ctx->allocations.backing;
    ctx->cache = gasset_Cache_create(ctx->alloc);
    ctx->reentrant.cache = ctx->cache;
    if (!gunit_check("cache created", ctx->cache != NULL)) {
        return false;
    }
    if (!gunit_check(
            "register reentrant asset type",
            gasset_Cache_register(
                ctx->alloc,
                ctx->cache,
                "outer",
                sizeof(int),
                gasset__test_reentrant_load,
                reload ? gasset__test_reentrant_reload : NULL,
                gasset__test_reentrant_unload,
                &ctx->reentrant
            )
        )) {
        return false;
    }

    return true;
}

static bool gasset__test_create_modules(gasset__TestContext* ctx) {
    if (!gasset__test_create(ctx)) {
        return false;
    }
    ctx->world = gecs_World_new(ctx->alloc);
    if (!gunit_check("module world created", ctx->world != NULL)) {
        return false;
    }
    ctx->modules = gmod_Modules_new(ctx->world);
    if (!gunit_check("module registry created", ctx->modules != NULL)) {
        return false;
    }

    return true;
}

static bool gasset__test_watch_fixture(gasset__TestContext* ctx) {
    if (!gasset__test_write_asset("glint-cache-test.lua", "one")) {
        return false;
    }
    if (!gasset__test_write_asset("glint-cache-test-extra.lua", "one")) {
        return false;
    }
    if (!gunit_check(
            "watch fixture",
            gasset_Cache_watch(ctx->alloc, ctx->cache, "fake", "/a", "glint-cache-test.lua")
        )) {
        return false;
    }
    if (!gunit_check("load watched asset", gasset_Cache_load(ctx->alloc, ctx->cache, "fake", "/a") != NULL)) {
        return false;
    }
    if (!gunit_check("load unwatched asset", gasset_Cache_load(ctx->alloc, ctx->cache, "fake", "/b") != NULL)) {
        return false;
    }

    return true;
}

static void gasset__test_destroy(void* user) {
    gasset__TestContext* ctx = user;
    if (ctx->modules != NULL) {
        gmod_Modules_free(ctx->alloc, ctx->modules);
    }
    if (ctx->cache != NULL) {
        ctx->allocations.fail_realloc = false;
        gasset_Cache_destroy(ctx->alloc, ctx->cache);
    }
    if (ctx->host_vfs.vfs != NULL) {
        gvfs_destroy(ctx->alloc, ctx->host_vfs.vfs);
    }
    if (ctx->world != NULL) {
        gecs_World_free(ctx->world);
    }
    remove("glint-cache-test.lua");
    remove("glint-cache-test-extra.lua");
}

static bool gasset__test_fake_load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    gasset__TestContext* ctx = user;
    ctx->load_attempts += 1;
    if (ctx->fail_loads) {
        return false;
    }

    gasset__TestFakeAsset* asset = instance;
    size_t length = strlen(path);
    asset->text = galloc_alloc(alloc, length + 1);
    if (asset->text == NULL) {
        return false;
    }

    memcpy(asset->text, path, length + 1);
    ctx->loads += 1;
    asset->generation = ctx->loads;
    return true;
}

static bool gasset__test_fake_reload(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    (void)alloc;
    gasset__TestContext* ctx = user;
    if (ctx->fail_reloads) {
        return false;
    }
    // Reloads always target the entry's own path, so the existing storage
    // from load still fits.
    gasset__TestFakeAsset* asset = instance;
    memcpy(asset->text, path, strlen(path) + 1);
    ctx->reloads += 1;
    asset->generation = ctx->reloads;
    return true;
}

static void gasset__test_fake_unload(galloc_Allocator alloc, void* instance, void* user) {
    (void)alloc;
    gasset__TestContext* ctx = user;
    gasset__TestFakeAsset* asset = instance;
    if (gunit_check("unload count leaves room", ctx->unload_count < 8)) {
        ctx->unload_order[ctx->unload_count] = asset;
        ctx->unload_count += 1;
    }
    galloc_free(alloc, asset->text);
    ctx->unloads += 1;
}

static bool gasset__test_write_asset(const char* path, const char* data) {
    FILE* file = fopen(path, "wb");
    if (!gunit_check("open fixture", file != NULL)) {
        return false;
    }
    size_t written = fwrite(data, 1, strlen(data), file);
    int closed = fclose(file);
    if (!gunit_check("write fixture contents", written == strlen(data))) {
        return false;
    }
    if (!gunit_check("close fixture", closed == 0)) {
        return false;
    }

    return true;
}

static bool gasset__test_reentrant_work(galloc_Allocator alloc, const char* path, gasset__TestReentrantContext* ctx) {
    ctx->callbacks += 1;
    if (!gunit_check("recursive load rejected", gasset_Cache_load(alloc, ctx->cache, "outer", path) == NULL)) {
        return false;
    }
    if (!gunit_check("recursive get rejected", gasset_Cache_get(ctx->cache, "outer", path) == NULL)) {
        return false;
    }
    if (!gunit_check("recursive reload rejected", !gasset_Cache_reload(alloc, ctx->cache, "outer", path))) {
        return false;
    }
    gasset_Cache_destroy(alloc, ctx->cache);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "type%d", i);
        if (ctx->callbacks == 1) {
            if (!gunit_check(
                    "register in callback",
                    gasset_Cache_register(
                        alloc,
                        ctx->cache,
                        name,
                        sizeof(int),
                        gasset__test_reentrant_load,
                        NULL,
                        NULL,
                        ctx
                    )
                )) {
                return false;
            }
        }

        snprintf(name, sizeof(name), "dep%zu-%d", ctx->callbacks, i);
        if (!gunit_check("dependency loaded", gasset_Cache_load(alloc, ctx->cache, "outer", name) != NULL)) {
            return false;
        }
    }

    return true;
}

static bool gasset__test_reentrant_load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    gasset__TestReentrantContext* ctx = user;
    *(int*)instance = 42;
    if (ctx->trigger && strcmp(path, "main") == 0) {
        if (!gasset__test_reentrant_work(alloc, path, ctx)) {
            return false;
        }
        if (ctx->fail_realloc != NULL) {
            *ctx->fail_realloc = true;
        }

        return !ctx->fail;
    }

    return true;
}

static bool gasset__test_reentrant_reload(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    gasset__TestReentrantContext* ctx = user;
    if (!gasset__test_reentrant_work(alloc, path, ctx)) {
        return false;
    }
    *(int*)instance = 99;
    return !ctx->fail;
}

static void gasset__test_reentrant_unload(galloc_Allocator alloc, void* instance, void* user) {
    gasset__TestReentrantContext* ctx = user;
    (void)instance;
    ctx->unloads += 1;
    gasset_Cache_destroy(alloc, ctx->cache);
}

static void* gasset__test_failing_alloc(void* data, size_t size) {
    return galloc_alloc(((gasset__TestFailingAllocator*)data)->backing, size);
}

static void gasset__test_failing_free(void* data, void* ptr) {
    galloc_free(((gasset__TestFailingAllocator*)data)->backing, ptr);
}

static void* gasset__test_failing_calloc(void* data, size_t count, size_t size) {
    return galloc_calloc(((gasset__TestFailingAllocator*)data)->backing, count, size);
}

static void* gasset__test_failing_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gasset__TestFailingAllocator* state = data;
    if (state->fail_realloc) {
        state->denied += 1;
        return NULL;
    }
    return galloc_realloc(state->backing, ptr, old_size, new_size);
}

static bool gasset__test_module_load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    (void)alloc;
    (void)instance;
    (void)path;
    (void)user;
    return true;
}

static void gasset__test_module_unload(galloc_Allocator alloc, void* instance, void* user) {
    (void)alloc;
    (void)instance;
    gasset__TestModuleState* state = user;
    gvfsmod_Service* vfs = gecs_context_get(state->world, state->vfs_type);
    if (!gunit_check("VFS survives asset unload", vfs != NULL && vfs->vfs != NULL)) {
        return;
    }
    state->unloaded += 1;
}

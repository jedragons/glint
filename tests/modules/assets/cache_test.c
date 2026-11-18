#include <glint/config.h>
#include <glint/modules/assets/cache.h>
#include <glint/modules/assets/module.h>
#include <glint/modules/vfs/module.h>
#include <glint/unit/unit.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if GLINT_PLATFORM_WINDOWS
#include <direct.h>
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

enum { GASSET_TEST_FIXTURE_DIR_CAPACITY = 64 };

// A fake asset type: an owned string plus a load counter, letting tests
// observe cache hits, reloads, failures and teardown order.
typedef struct gasset__TestFakeAsset {
    char* text;
    size_t load_id;
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
    size_t unload_order[8];
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
    bool owns_fixture_dir;
    char fixture_dir[GASSET_TEST_FIXTURE_DIR_CAPACITY];
    char fixture_path[GASSET_TEST_FIXTURE_DIR_CAPACITY + sizeof("/asset.lua")];
    char extra_fixture_path[GASSET_TEST_FIXTURE_DIR_CAPACITY + sizeof("/extra.lua")];
} gasset__TestContext;

static bool gasset__test_create(gasset__TestContext* ctx);
static bool gasset__test_create_registered(gasset__TestContext* ctx, bool reload);
static bool gasset__test_create_reentrant(gasset__TestContext* ctx, bool reload, bool failing);
static bool gasset__test_create_modules(gasset__TestContext* ctx);
static void gasset__test_destroy(void* user);
static bool gasset__test_register(gasset__TestContext* ctx, const char* name, bool reload);
static bool gasset__test_create_fixture_dir(gasset__TestContext* ctx);
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

GUNIT_TEST(asset_registration,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create cache", gasset__test_create(&ctx));
    })

    GUNIT_SECTION("rejects a duplicate type name", {
        gunit_assert("register type", gasset__test_register(&ctx, "fake", true));
        gunit_assert("duplicate type rejected", !gasset__test_register(&ctx, "fake", true));
    })

    GUNIT_SECTION("rejects an empty type name", {
        gunit_assert("empty type rejected", !gasset__test_register(&ctx, "", true));
    })

    GUNIT_SECTION("requires a load callback", {
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
    })

    GUNIT_SECTION("requires a nonzero instance size", {
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
    })
)

GUNIT_TEST(asset_unregister,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("register asset type", gasset__test_create_registered(&ctx, true));
    })

    GUNIT_SECTION("unloads only the selected type", {
        gunit_assert("second type registered", gasset__test_register(&ctx, "second", true));
        gasset__TestFakeAsset* first = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/one");
        void* second = gasset_Cache_load(ctx.alloc, ctx.cache, "second", "/two");
        gunit_assert("both loaded", first != NULL && second != NULL);
        const size_t first_id = first->load_id;
        gunit_assert("type removed", gasset_Cache_unregister(ctx.alloc, ctx.cache, "fake"));
        gunit_assert("only removed type unloaded", ctx.unloads == 1 && ctx.unload_order[0] == first_id);
        gunit_assert("other type remains accessible", gasset_Cache_get(ctx.cache, "second", "/two") == second);
    })

    GUNIT_SECTION("releases the type name for reuse", {
        gunit_assert("type removed", gasset_Cache_unregister(ctx.alloc, ctx.cache, "fake"));
        gunit_assert("removed name is reusable", gasset__test_register(&ctx, "fake", false));
    })
)

GUNIT_TEST(asset_empty_cache,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create cache", gasset__test_create(&ctx));
    })

    GUNIT_SECTION("loading an unknown type fails", {
        gunit_assert("unknown type does not load", gasset_Cache_load(ctx.alloc, ctx.cache, "missing", "/a") == NULL);
    })

    GUNIT_SECTION("destruction has nothing to unload", {
        gasset_Cache_destroy(ctx.alloc, ctx.cache);
        ctx.cache = NULL;
        gunit_assert("nothing to unload", ctx.unload_count == 0);
    })
)

GUNIT_TEST(asset_load_and_lookup,
    gasset__TestContext ctx = {0};
    gasset__TestFakeAsset* asset = NULL;

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("register asset type", gasset__test_create_registered(&ctx, true));
        asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua");
        gunit_assert("asset loaded", asset != NULL);
    })

    GUNIT_SECTION("contains the data supplied by the loader", {
        gunit_assert(
            "asset contents loaded",
            asset->text != NULL && strcmp(asset->text, "/assets/hero.lua") == 0
        );
        gunit_assert("load ran once", ctx.loads == 1);
    })

    GUNIT_SECTION("loading the same path reuses the instance", {
        gunit_assert(
            "cache hit returns same instance",
            gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/hero.lua") == asset
        );
        gunit_assert("cache hit does not reload", ctx.loads == 1);
    })

    GUNIT_SECTION("loading a different path creates another instance", {
        void* second = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/assets/map.lua");
        gunit_assert(
            "different paths produce different instances",
            second != NULL && second != asset && ctx.loads == 2
        );
    })

    GUNIT_SECTION("lookup finds only loaded paths", {
        gunit_assert("contains finds cached assets", gasset_Cache_contains(ctx.cache, "fake", "/assets/hero.lua"));
        gunit_assert("get returns cached instance", gasset_Cache_get(ctx.cache, "fake", "/assets/hero.lua") == asset);
        gunit_assert("contains misses uncached assets", !gasset_Cache_contains(ctx.cache, "fake", "/assets/nope.lua"));
        gunit_assert("get misses uncached assets", gasset_Cache_get(ctx.cache, "fake", "/assets/nope.lua") == NULL);
    })
)

GUNIT_TEST(asset_destroy_unloads_in_reverse_load_order,
    gasset__TestContext ctx = {0};
    gunit_defer(gasset__test_destroy, &ctx);
    gunit_assert("create registered", gasset__test_create_registered(&ctx, true));

    gasset__TestFakeAsset* first = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/one");
    gasset__TestFakeAsset* second = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/two");
    gunit_assert("both assets loaded", first != NULL && second != NULL);
    const size_t first_id = first->load_id;
    const size_t second_id = second->load_id;
    gasset_Cache_destroy(ctx.alloc, ctx.cache);
    ctx.cache = NULL;
    gunit_assert("every asset unloaded", ctx.unloads == 2);
    gunit_assert("later asset unloaded first", ctx.unload_order[0] == second_id && ctx.unload_order[1] == first_id);
)

GUNIT_TEST(asset_failed_load,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("register asset type", gasset__test_create_registered(&ctx, true));
        ctx.fail_loads = true;
        gunit_assert("initial load fails", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad") == NULL);
    })

    GUNIT_SECTION("leaves the path uncached", {
        gunit_assert("failed load is not cached", !gasset_Cache_contains(ctx.cache, "fake", "/bad"));
    })

    GUNIT_SECTION("retries the loader until the asset is available", {
        gunit_assert("failed load may be retried", gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad") == NULL);
        gunit_assert("retries reach loader", ctx.load_attempts == 2);
        ctx.fail_loads = false;
        gasset__TestFakeAsset* asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/bad");
        gunit_assert("retry succeeds", asset != NULL && ctx.loads == 1 && ctx.load_attempts == 3);
        const size_t load_id = asset->load_id;
        gasset_Cache_destroy(ctx.alloc, ctx.cache);
        ctx.cache = NULL;
        gunit_assert("only successful asset unloaded", ctx.unload_count == 1 && ctx.unload_order[0] == load_id);
    })
)

GUNIT_TEST(asset_reload,
    gasset__TestContext ctx = {0};
    gasset__TestFakeAsset* asset = NULL;

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("register asset type", gasset__test_create_registered(&ctx, true));
        asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
        gunit_assert("asset loaded", asset != NULL);
    })

    GUNIT_SECTION("reload succeeds", {
        gunit_assert("reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
        gunit_assert("reload retains instance", gasset_Cache_get(ctx.cache, "fake", "/a") == asset);
        gunit_assert("reload keeps the asset path", asset->text != NULL && strcmp(asset->text, "/a") == 0);
        gunit_assert("reload advances the generation", asset->generation == 2);
        gunit_assert("reload ran once", ctx.reloads == 1 && ctx.loads == 1);
    })

    GUNIT_SECTION("reload fails", {
        ctx.fail_reloads = true;
        gunit_assert("failed reload reported", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
        gunit_assert("failed reload retains instance", gasset_Cache_get(ctx.cache, "fake", "/a") == asset);
        gunit_assert("old path preserved", asset->text != NULL && strcmp(asset->text, "/a") == 0);
        gunit_assert("generation unchanged", asset->generation == 1);
        gunit_assert("failed reload stays cached", gasset_Cache_contains(ctx.cache, "fake", "/a"));
    })

    GUNIT_SECTION("rejects an unloaded path", {
        gunit_assert("reload without load fails", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/missing"));
    })
)

GUNIT_TEST(asset_fallback_reload,
    gasset__TestContext ctx = {0};
    gasset__TestFakeAsset* asset = NULL;

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("register asset type", gasset__test_create_registered(&ctx, false));
        asset = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
        gunit_assert("asset loaded", asset != NULL);
    })

    GUNIT_SECTION("reload succeeds in place", {
        gunit_assert("fallback reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
        gunit_assert("fallback unloads and loads", ctx.unloads == 1 && ctx.loads == 2);
        gunit_assert("instance stays in place", gasset_Cache_get(ctx.cache, "fake", "/a") == asset);
    })

    GUNIT_SECTION("failed load evicts the stale instance", {
        ctx.fail_loads = true;
        gunit_assert("failed fallback reload reported", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
        gunit_assert("evicted asset is gone", !gasset_Cache_contains(ctx.cache, "fake", "/a"));
        gunit_assert("stale instance unloaded", ctx.unload_count == 1);
    })

    GUNIT_SECTION("an evicted path can be loaded again", {
        gunit_assert("initial reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
        ctx.fail_loads = true;
        gunit_assert("failed reload evicts", !gasset_Cache_reload(ctx.alloc, ctx.cache, "fake", "/a"));
        ctx.fail_loads = false;
        gasset__TestFakeAsset* replacement = gasset_Cache_load(ctx.alloc, ctx.cache, "fake", "/a");
        gunit_assert("evicted path loads again", replacement != NULL && ctx.loads == 3);
        const size_t load_id = replacement->load_id;
        gasset_Cache_destroy(ctx.alloc, ctx.cache);
        ctx.cache = NULL;
        gunit_assert("surviving instance unloaded last", ctx.unload_count == 3 && ctx.unload_order[2] == load_id);
    })
)

GUNIT_TEST(asset_watch,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("register asset type", gasset__test_create_registered(&ctx, true));
        gunit_assert("watch fixture", gasset__test_watch_fixture(&ctx));
    })

    GUNIT_SECTION("rejects an unknown type", {
        gunit_assert(
            "unknown mapping rejected",
            !gasset_Cache_watch(ctx.alloc, ctx.cache, "missing", "/a", ctx.fixture_path)
        );
    })

    GUNIT_SECTION("an unchanged file does not trigger reload", {
        gasset_Cache_poll(ctx.alloc, ctx.cache);
        gunit_assert("unchanged files do not reload", ctx.reloads == 0);
    })

    GUNIT_SECTION("a changed file reloads only its mapped asset", {
        gunit_assert("write asset", gasset__test_write_asset(ctx.fixture_path, "changed contents"));
        gasset_Cache_poll(ctx.alloc, ctx.cache);
        gasset__TestFakeAsset* first = gasset_Cache_get(ctx.cache, "fake", "/a");
        gasset__TestFakeAsset* second = gasset_Cache_get(ctx.cache, "fake", "/b");
        gunit_assert("changed file reloads once", ctx.reloads == 1);
        gunit_assert(
            "mapped asset reloaded",
            first != NULL && first->text != NULL && strcmp(first->text, "/a") == 0 && first->generation == 2
        );
        gunit_assert("unmapped asset untouched", second != NULL && second->generation == 1 && ctx.loads == 2);
    })

    GUNIT_SECTION("watching a second file twice reloads the asset once", {
        gunit_assert(
            "second native file",
            gasset_Cache_watch(ctx.alloc, ctx.cache, "fake", "/a", ctx.extra_fixture_path)
        );
        gunit_assert(
            "duplicate watch accepted",
            gasset_Cache_watch(ctx.alloc, ctx.cache, "fake", "/a", ctx.extra_fixture_path)
        );
        gunit_assert("write asset", gasset__test_write_asset(ctx.extra_fixture_path, "second changed contents"));
        gasset_Cache_poll(ctx.alloc, ctx.cache);
        gunit_assert("second native file reloads once", ctx.reloads == 1);
    })

    GUNIT_SECTION("unregistering the type removes all its mappings", {
        gunit_assert(
            "second native file",
            gasset_Cache_watch(ctx.alloc, ctx.cache, "fake", "/a", ctx.extra_fixture_path)
        );
        gunit_assert("type unregistered", gasset_Cache_unregister(ctx.alloc, ctx.cache, "fake"));
        gunit_assert("write asset", gasset__test_write_asset(ctx.fixture_path, "changed after unregister"));
        gunit_assert("write asset", gasset__test_write_asset(ctx.extra_fixture_path, "changed after unregister"));
        gasset_Cache_poll(ctx.alloc, ctx.cache);
        gunit_assert("unregister removed every mapping", ctx.reloads == 0 && ctx.unload_count == 2);
    })
)

GUNIT_TEST(asset_reentrant_load,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create reentrant cache", gasset__test_create_reentrant(&ctx, false, false));
        ctx.reentrant.trigger = true;
    })

    GUNIT_SECTION("retains dependencies loaded in the callback", {
        int* primary = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
        gunit_assert("reentrant initial load", primary != NULL && *primary == 42 && ctx.reentrant.callbacks == 1);
        gunit_assert("dependency retained", gasset_Cache_get(ctx.cache, "outer", "dep1-2") != NULL);
    })

    GUNIT_SECTION("a failed initial load can be retried", {
        ctx.reentrant.fail = true;
        gunit_assert("initial load fails", gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main") == NULL);
        ctx.reentrant.fail = false;
        gunit_assert("retry succeeds", gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main") != NULL);
    })

    GUNIT_SECTION("fallback reload keeps the original instance", {
        void* primary = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
        gunit_assert("initial load", primary != NULL);
        gunit_assert("fallback reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
        gunit_assert("instance retained", gasset_Cache_get(ctx.cache, "outer", "main") == primary);
    })

    GUNIT_SECTION("failed fallback reload preserves dependencies", {
        gunit_assert("initial load", gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main") != NULL);
        ctx.reentrant.fail = true;
        gunit_assert("fallback reload fails", !gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
        gunit_assert("failed entry evicted", gasset_Cache_get(ctx.cache, "outer", "main") == NULL);
        gunit_assert("dependencies survive", gasset_Cache_get(ctx.cache, "outer", "dep2-2") != NULL);
    })
)

GUNIT_TEST(asset_reentrant_custom_reload,
    gasset__TestContext ctx = {0};
    int* primary = NULL;

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create reentrant cache", gasset__test_create_reentrant(&ctx, true, false));
        primary = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
        gunit_assert("initial load", primary != NULL);
    })

    GUNIT_SECTION("successful reload updates the instance", {
        gunit_assert("custom reload succeeds", gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
        gunit_assert("custom reload updates instance", *primary == 99);
    })

    GUNIT_SECTION("failed reload retains the entry", {
        ctx.reentrant.fail = true;
        gunit_assert("custom reload fails", !gasset_Cache_reload(ctx.alloc, ctx.cache, "outer", "main"));
        gunit_assert("failure retains entry", gasset_Cache_get(ctx.cache, "outer", "main") == primary);
    })
)

GUNIT_TEST(asset_reentrant_allocation_failure,
    gasset__TestContext ctx = {0};
    int* primary = NULL;

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create reentrant cache", gasset__test_create_reentrant(&ctx, false, true));
        ctx.reentrant.trigger = true;
        ctx.reentrant.fail_realloc = &ctx.allocations.fail_realloc;
        primary = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
    })

    GUNIT_SECTION("unregister unloads each instance once", {
        if (primary == NULL) {
            gunit_assert("failure follows an allocation denial", ctx.allocations.denied > 0);
            gunit_assert("loaded instance cleaned up", ctx.reentrant.unloads == 1);
            gunit_assert("failed instance not cached", gasset_Cache_get(ctx.cache, "outer", "main") == NULL);
        } else {
            gunit_assert(
                "successful load retains its instance",
                *primary == 42 && gasset_Cache_get(ctx.cache, "outer", "main") == primary
            );
            gunit_assert("cached instance remains owned", ctx.reentrant.unloads == 0);
        }
        gunit_assert("reentrant dependency survives", gasset_Cache_get(ctx.cache, "outer", "dep1-2") != NULL);
        ctx.allocations.fail_realloc = false;
        gunit_assert("type can be unloaded", gasset_Cache_unregister(ctx.alloc, ctx.cache, "outer"));
        gunit_assert("each loaded instance unloaded once", ctx.reentrant.unloads == 4);
    })

    GUNIT_SECTION("the path can still be loaded", {
        ctx.allocations.fail_realloc = false;
        ctx.reentrant.fail_realloc = NULL;
        ctx.reentrant.trigger = false;
        void* retried = gasset_Cache_load(ctx.alloc, ctx.cache, "outer", "main");
        gunit_assert("path remains loadable", retried != NULL);
        gunit_assert("successful prior load remains cached", primary == NULL || retried == primary);
    })
)

GUNIT_TEST(asset_module_owned_services,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create modules", gasset__test_create_modules(&ctx));
        gmod_Module assets = gasset_module();
        gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
        gunit_assert("load dependencies", gmod_Modules_load(ctx.alloc, ctx.modules));
    })

    GUNIT_SECTION("publishes its own VFS and cache", {
        gvfsmod_Service* vfs = gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gvfsmod_Service));
        gasset_Service* asset = gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gasset_Service));
        gunit_assert("owned VFS", vfs != NULL && vfs->vfs != NULL && vfs->module_owned);
        gunit_assert("owned cache", asset != NULL && asset->cache != NULL && asset->module_owned);
    })

    GUNIT_SECTION("unloads assets while the VFS is still alive", {
        gecs_Component vfs_type = GECS_CONTEXT(ctx.world, gvfsmod_Service);
        gecs_Component assets_type = GECS_CONTEXT(ctx.world, gasset_Service);
        gasset_Service* service = gecs_context_get(ctx.world, assets_type);
        gunit_assert("asset service exists", service != NULL);
        ctx.module_state = (gasset__TestModuleState) {.world = ctx.world, .vfs_type = vfs_type};
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
        gunit_assert(
            "load observed asset",
            gasset_Cache_load(ctx.alloc, service->cache, "module-test", "item") != NULL
        );
        gmod_Modules_update(ctx.alloc, ctx.modules);
        gunit_assert("unload dependencies", gmod_Modules_unload(ctx.alloc, ctx.modules));
        gunit_assert("asset unloaded while VFS alive", ctx.module_state.unloaded == 1);
        gunit_assert(
            "owned services removed",
            gecs_context_get(ctx.world, vfs_type) == NULL && gecs_context_get(ctx.world, assets_type) == NULL
        );
    })
)

GUNIT_TEST(asset_module_host_services,
    gasset__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gasset__test_destroy, &ctx);
        gunit_assert("create modules", gasset__test_create_modules(&ctx));
    })

    GUNIT_SECTION("preserves borrowed services on unload", {
        ctx.host_vfs.vfs = gvfs_create(ctx.alloc);
        ctx.host_assets.cache = ctx.cache;
        gecs_Component vfs_type = GECS_CONTEXT(ctx.world, gvfsmod_Service);
        gecs_Component assets_type = GECS_CONTEXT(ctx.world, gasset_Service);
        gunit_assert("create host VFS", ctx.host_vfs.vfs != NULL);
        gunit_assert("provide VFS", gecs_context_set(ctx.world, vfs_type, &ctx.host_vfs) != NULL);
        gunit_assert("provide cache", gecs_context_set(ctx.world, assets_type, &ctx.host_assets) != NULL);
        gmod_Module assets = gasset_module();
        gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
        gunit_assert("borrow services", gmod_Modules_load(ctx.alloc, ctx.modules));
        gunit_assert("unload module", gmod_Modules_unload(ctx.alloc, ctx.modules));
        gvfsmod_Service* vfs = gecs_context_get(ctx.world, vfs_type);
        gasset_Service* assets_service = gecs_context_get(ctx.world, assets_type);
        gunit_assert("borrowed VFS preserved", vfs != NULL && vfs->vfs == ctx.host_vfs.vfs);
        gunit_assert("borrowed cache preserved", assets_service != NULL && assets_service->cache == ctx.cache);
    })

    GUNIT_SECTION("rolls back the VFS dependency for an invalid host cache", {
        gecs_Component assets_type = GECS_CONTEXT(ctx.world, gasset_Service);
        gunit_assert("set invalid cache", gecs_context_set(ctx.world, assets_type, &ctx.host_assets) != NULL);
        gmod_Module assets = gasset_module();
        gunit_assert("import assets", gmod_Modules_import(ctx.alloc, ctx.modules, assets));
        gunit_assert("invalid host cache fails", !gmod_Modules_load(ctx.alloc, ctx.modules));
        gunit_assert(
            "dependency rolled back",
            gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gvfsmod_Service)) == NULL
        );
    })
)

gunit_main();

static bool gasset__test_create(gasset__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    ctx->cache = gasset_Cache_create(ctx->alloc);
    return gunit_check("cache created", ctx->cache != NULL);
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
    return gunit_check("register fake asset type", gasset__test_register(ctx, "fake", reload));
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
    return gunit_check(
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
    );
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
    return gunit_check("module registry created", ctx->modules != NULL);
}

static bool gasset__test_create_fixture_dir(gasset__TestContext* ctx) {
#if GLINT_PLATFORM_WINDOWS
    const long pid = (long)_getpid();
#else
    const long pid = (long)getpid();
#endif
    // Claim a new directory atomically; a previous run may have left a name behind.
    for (size_t attempt = 0; attempt < 128; attempt++) {
        snprintf(ctx->fixture_dir, sizeof(ctx->fixture_dir), "glint-cache-test-%ld-%zu", pid, attempt);
#if GLINT_PLATFORM_WINDOWS
        int created = _mkdir(ctx->fixture_dir);
#else
        int created = mkdir(ctx->fixture_dir, 0700);
#endif
        if (created == 0) {
            ctx->owns_fixture_dir = true;
            return true;
        }
        if (!gunit_check("create watch fixture directory", errno == EEXIST)) {
            return false;
        }
    }

    return gunit_check("watch fixture directory name available", false);
}

static bool gasset__test_watch_fixture(gasset__TestContext* ctx) {
    if (!gasset__test_create_fixture_dir(ctx)) {
        return false;
    }
    snprintf(ctx->fixture_path, sizeof(ctx->fixture_path), "%s/asset.lua", ctx->fixture_dir);
    snprintf(ctx->extra_fixture_path, sizeof(ctx->extra_fixture_path), "%s/extra.lua", ctx->fixture_dir);

    if (!gasset__test_write_asset(ctx->fixture_path, "one")) {
        return false;
    }
    if (!gasset__test_write_asset(ctx->extra_fixture_path, "one")) {
        return false;
    }
    if (!gunit_check("watch fixture", gasset_Cache_watch(ctx->alloc, ctx->cache, "fake", "/a", ctx->fixture_path))) {
        return false;
    }
    if (!gunit_check("load watched asset", gasset_Cache_load(ctx->alloc, ctx->cache, "fake", "/a") != NULL)) {
        return false;
    }
    return gunit_check("load unwatched asset", gasset_Cache_load(ctx->alloc, ctx->cache, "fake", "/b") != NULL);
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
    if (ctx->owns_fixture_dir) {
        remove(ctx->fixture_path);
        remove(ctx->extra_fixture_path);
#if GLINT_PLATFORM_WINDOWS
        int removed = _rmdir(ctx->fixture_dir);
#else
        int removed = rmdir(ctx->fixture_dir);
#endif
        gunit_check("remove watch fixture directory", removed == 0);
    }
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
    asset->load_id = ctx->loads;
    asset->generation = 1;
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
    asset->generation += 1;
    return true;
}

static void gasset__test_fake_unload(galloc_Allocator alloc, void* instance, void* user) {
    gasset__TestContext* ctx = user;
    gasset__TestFakeAsset* asset = instance;
    if (gunit_check("unload count leaves room", ctx->unload_count < 8)) {
        ctx->unload_order[ctx->unload_count] = asset->load_id;
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
    return gunit_check("close fixture", closed == 0);
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

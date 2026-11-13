#include <glint/assets/cache.h>

#include <mewunit.h>
#include <stdio.h>
#include <string.h>

/* A fake asset type: an owned string plus a load counter, letting tests
 * observe cache hits, reloads, failures and teardown order. */
typedef struct gasset__TestFakeAsset {
    char *text;
    unsigned generation;
} gasset__TestFakeAsset;

static unsigned gasset__test_fake_loads, gasset__test_fake_reloads, gasset__test_fake_unloads;
static unsigned gasset__test_fake_load_attempts;
static const gasset__TestFakeAsset *gasset__test_fake_unload_order[8];
static unsigned gasset__test_fake_unload_count;
static int gasset__test_fake_fail_loads, gasset__test_fake_fail_reloads;

static void gasset__test_fake_reset(void) {
    gasset__test_fake_loads = gasset__test_fake_reloads = gasset__test_fake_unloads = gasset__test_fake_unload_count = gasset__test_fake_load_attempts = 0;
    gasset__test_fake_fail_loads = gasset__test_fake_fail_reloads = 0;
    memset(gasset__test_fake_unload_order, 0, sizeof(gasset__test_fake_unload_order));
}

static int gasset__test_fake_load(galloc_Allocator alloc, void *instance, const char *path, void *user) {
    (void)user;
    gasset__test_fake_load_attempts++;
    if (gasset__test_fake_fail_loads) {
        return 0;
    }
    gasset__TestFakeAsset *asset = instance;
    size_t length = strlen(path);
    asset->text = galloc_alloc(alloc, length + 1);
    if (!asset->text) {
        return 0;
    }
    memcpy(asset->text, path, length + 1);
    asset->generation = ++gasset__test_fake_loads;
    return 1;
}

static int gasset__test_fake_reload(galloc_Allocator alloc, void *instance, const char *path, void *user) {
    (void)alloc;
    (void)user;
    if (gasset__test_fake_fail_reloads) {
        return 0;
    }
    /* Reloads always target the entry's own path, so the existing storage
     * from load still fits. */
    gasset__TestFakeAsset *asset = instance;
    memcpy(asset->text, path, strlen(path) + 1);
    asset->generation = ++gasset__test_fake_reloads;
    return 1;
}

static void gasset__test_fake_unload(galloc_Allocator alloc, void *instance, void *user) {
    (void)alloc;
    (void)user;
    gasset__TestFakeAsset *asset = instance;
    mewrequire("unload count leaves room", gasset__test_fake_unload_count < 8);
    gasset__test_fake_unload_order[gasset__test_fake_unload_count++] = asset;
    galloc_free(alloc, asset->text);
    gasset__test_fake_unloads++;
}

TEST(asset_cache_types, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset_Cache *cache = gasset_Cache_create(alloc);
    mewassert("cache was created", cache);
    int marker = 0;
    mewassert("type registered", gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load,
                                                            gasset__test_fake_reload, gasset__test_fake_unload, &marker));
    mewassert("duplicate type rejected", !gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load,
                                                                     gasset__test_fake_reload, gasset__test_fake_unload, &marker));
    mewassert("empty type rejected", !gasset_Cache_register(alloc, cache, "", sizeof(gasset__TestFakeAsset), gasset__test_fake_load,
                                                                 gasset__test_fake_reload, gasset__test_fake_unload, &marker));
    mewassert("missing load rejected", !gasset_Cache_register(alloc, cache, "other", sizeof(gasset__TestFakeAsset), NULL,
                                                                   gasset__test_fake_reload, gasset__test_fake_unload, &marker));
    mewassert("unknown type does not load", !gasset_Cache_load(alloc, cache, "missing", "/a"));
    mewassert("zero instance size rejected", !gasset_Cache_register(alloc, cache, "tiny", 0, gasset__test_fake_load, gasset__test_fake_reload,
                                                                         gasset__test_fake_unload, &marker));
    gasset_Cache_destroy(alloc, cache);
    mewassert("nothing to unload", gasset__test_fake_unload_count == 0);
})

TEST(asset_cache_load, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset_Cache *cache = gasset_Cache_create(alloc);
    mewassert("Test expectation failed",
              gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load, gasset__test_fake_reload, gasset__test_fake_unload, NULL));
    gasset__test_fake_reset();

    gasset__TestFakeAsset *asset = gasset_Cache_load(alloc, cache, "fake", "/assets/hero.lua");
    mewassert("asset loaded", asset && asset->text && strcmp(asset->text, "/assets/hero.lua") == 0);
    mewassert("load ran once", gasset__test_fake_loads == 1);

    gasset__TestFakeAsset *again = gasset_Cache_load(alloc, cache, "fake", "/assets/hero.lua");
    mewassert("cache hit returns the same pointer", again == asset);
    mewassert("cache hit does not reload", gasset__test_fake_loads == 1);

    gasset__TestFakeAsset *other = gasset_Cache_load(alloc, cache, "fake", "/assets/map.lua");
    mewassert("different path is a different instance", other && other != asset && gasset__test_fake_loads == 2);

    mewassert("contains finds cached assets", gasset_Cache_contains(cache, "fake", "/assets/hero.lua"));
    mewassert("contains misses uncached assets", !gasset_Cache_contains(cache, "fake", "/assets/nope.lua"));
    mewassert("get returns the cached instance", gasset_Cache_get(cache, "fake", "/assets/hero.lua") == asset);
    mewassert("get returns nothing before a load", gasset_Cache_get(cache, "fake", "/assets/nope.lua") == NULL);

    gasset_Cache_destroy(alloc, cache);
    mewassert("every asset was unloaded", gasset__test_fake_unloads == 2);
    /* Reverse load order. */
    mewassert("later asset unloaded first", gasset__test_fake_unload_order[0] == other && gasset__test_fake_unload_order[1] == asset);
})

TEST(asset_cache_failure, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset_Cache *cache = gasset_Cache_create(alloc);
    mewassert("Test expectation failed",
              gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load, gasset__test_fake_reload, gasset__test_fake_unload, NULL));
    gasset__test_fake_reset();

    gasset__test_fake_fail_loads = 1;
    mewassert("failed load returns nothing", !gasset_Cache_load(alloc, cache, "fake", "/bad"));
    mewassert("failed load is not cached", !gasset_Cache_contains(cache, "fake", "/bad"));
    mewassert("failed load may be retried", !gasset_Cache_load(alloc, cache, "fake", "/bad"));
    mewassert("retries reached the loader", gasset__test_fake_load_attempts == 2);

    gasset__test_fake_fail_loads = 0;
    gasset__TestFakeAsset *asset = gasset_Cache_load(alloc, cache, "fake", "/good");
    mewassert("retry succeeds", asset && gasset__test_fake_loads == 1 && gasset__test_fake_load_attempts == 3);
    gasset_Cache_destroy(alloc, cache);
    mewassert("only the successful asset unloaded", gasset__test_fake_unload_count == 1 && gasset__test_fake_unload_order[0] == asset);
})

TEST(asset_cache_reload, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset_Cache *cache = gasset_Cache_create(alloc);
    mewassert("Test expectation failed",
              gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load, gasset__test_fake_reload, gasset__test_fake_unload, NULL));
    gasset__test_fake_reset();

    mewassert("reload without a load fails", !gasset_Cache_reload(alloc, cache, "fake", "/a"));
    gasset__TestFakeAsset *asset = gasset_Cache_load(alloc, cache, "fake", "/a");
    mewassert("Test expectation failed", asset);
    mewassert("reload keeps the instance", gasset_Cache_reload(alloc, cache, "fake", "/a"));
    mewassert("reload updated in place", asset->text && strcmp(asset->text, "/a") == 0 && asset->generation == 1);
    mewassert("reload ran once", gasset__test_fake_reloads == 1 && gasset__test_fake_loads == 1);

    gasset__test_fake_fail_reloads = 1;
    mewassert("failed reload reports failure", !gasset_Cache_reload(alloc, cache, "fake", "/a"));
    mewassert("failed reload keeps the old contents", strcmp(asset->text, "/a") == 0 && asset->generation == 1);
    mewassert("failed reload stays cached", gasset_Cache_contains(cache, "fake", "/a"));
    gasset__test_fake_fail_reloads = 0;
    gasset_Cache_destroy(alloc, cache);
})

static void gasset__test_write_asset(const char *path, const char *data) {
    FILE *file = fopen(path, "wb");
    mewassert("Test expectation failed", file);
    mewassert("Test expectation failed", fwrite(data, 1, strlen(data), file) == strlen(data));
    mewassert("Test expectation failed", fclose(file) == 0);
}

TEST(asset_cache_watch, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset_Cache *cache = gasset_Cache_create(alloc);
    mewassert("Test expectation failed",
              gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load, gasset__test_fake_reload, gasset__test_fake_unload, NULL));
    gasset__test_fake_reset();
    gasset__test_write_asset("glint-cache-test.lua", "one");

    mewassert("watched asset", gasset_Cache_watch(alloc, cache, "fake", "/a", "glint-cache-test.lua"));
    gasset__TestFakeAsset *first = gasset_Cache_load(alloc, cache, "fake", "/a");
    gasset__TestFakeAsset *second = gasset_Cache_load(alloc, cache, "fake", "/b");
    mewassert("Test expectation failed", first && second);
    mewassert("unwatched mapping rejected", !gasset_Cache_watch(alloc, cache, "missing", "/a", "glint-cache-test.lua"));

    gasset_Cache_poll(alloc, cache);
    mewassert("unchanged files do not reload", gasset__test_fake_reloads == 0);

    gasset__test_write_asset("glint-cache-test.lua", "changed contents");
    gasset_Cache_poll(alloc, cache);
    mewassert("changed file reloads", gasset__test_fake_reloads == 1);
    mewassert("reload touched the mapped asset", strcmp(first->text, "/a") == 0 && first->generation == 1);
    mewassert("unmapped asset untouched", second->generation == 2 && gasset__test_fake_loads == 2);

    gasset_Cache_destroy(alloc, cache);
    mewassert("watched assets unload on destroy", gasset__test_fake_unload_count == 2);
})

TEST(asset_cache_fallback_reload, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset_Cache *cache = gasset_Cache_create(alloc);
    /* No reload callback: reload falls back to unload + load. */
    mewassert("Test expectation failed",
              gasset_Cache_register(alloc, cache, "fake", sizeof(gasset__TestFakeAsset), gasset__test_fake_load, NULL, gasset__test_fake_unload, NULL));
    gasset__test_fake_reset();

    gasset__TestFakeAsset *asset = gasset_Cache_load(alloc, cache, "fake", "/a");
    mewassert("Test expectation failed", asset);
    mewassert("fallback reload succeeds", gasset_Cache_reload(alloc, cache, "fake", "/a"));
    mewassert("fallback unloaded and loaded", gasset__test_fake_unloads == 1 && gasset__test_fake_loads == 2);
    mewassert("instance stays in place", gasset_Cache_get(cache, "fake", "/a") == asset);

    gasset__test_fake_fail_loads = 1;
    mewassert("failed fallback reload evicts", !gasset_Cache_reload(alloc, cache, "fake", "/a"));
    mewassert("evicted asset is gone", !gasset_Cache_contains(cache, "fake", "/a"));
    mewassert("eviction unloaded the stale instance", gasset__test_fake_unload_count == 2);
    gasset__test_fake_fail_loads = 0;

    gasset__TestFakeAsset *reloaded = gasset_Cache_load(alloc, cache, "fake", "/a");
    mewassert("evicted path loads again", reloaded && gasset__test_fake_loads == 3);
    gasset_Cache_destroy(alloc, cache);
    mewassert("the surviving instance unloaded last", gasset__test_fake_unload_count == 3 && gasset__test_fake_unload_order[2] == reloaded);
})

typedef struct gasset__TestReentrantContext {
    gasset_Cache *cache;
    int trigger, fail, callbacks, unloads;
    int *fail_realloc;
} gasset__TestReentrantContext;

static int gasset__test_reentrant_load(galloc_Allocator alloc, void *instance, const char *path, void *user);

static void gasset__test_reentrant_work(galloc_Allocator alloc, const char *path, gasset__TestReentrantContext *ctx) {
    ctx->callbacks++;
    mewassert("recursive load rejected", !gasset_Cache_load(alloc, ctx->cache, "outer", path));
    mewassert("recursive get rejected", !gasset_Cache_get(ctx->cache, "outer", path));
    mewassert("recursive reload rejected", !gasset_Cache_reload(alloc, ctx->cache, "outer", path));
    gasset_Cache_destroy(alloc, ctx->cache);
    for (int i = 0; i < 8; i++) {
        char name[32];
        snprintf(name, sizeof(name), "type%d", i);
        if (ctx->callbacks == 1) {
            mewassert("register in callback", gasset_Cache_register(alloc, ctx->cache, name, sizeof(int),
                                                                        gasset__test_reentrant_load, NULL, NULL, ctx));
        }
        snprintf(name, sizeof(name), "dep%d-%d", ctx->callbacks, i);
        mewassert("dependency loaded", gasset_Cache_load(alloc, ctx->cache, "outer", name));
    }
}

static int gasset__test_reentrant_load(galloc_Allocator alloc, void *instance, const char *path, void *user) {
    gasset__TestReentrantContext *ctx = user;
    *(int *)instance = 42;
    if (ctx->trigger && strcmp(path, "main") == 0) {
        gasset__test_reentrant_work(alloc, path, ctx);
        if (ctx->fail_realloc) {
            *ctx->fail_realloc = 1;
        }
        return !ctx->fail;
    }
    return 1;
}

static int gasset__test_reentrant_reload(galloc_Allocator alloc, void *instance, const char *path, void *user) {
    gasset__TestReentrantContext *ctx = user;
    gasset__test_reentrant_work(alloc, path, ctx);
    *(int *)instance = 99;
    return !ctx->fail;
}

static void gasset__test_reentrant_unload(galloc_Allocator alloc, void *instance, void *user) {
    gasset__TestReentrantContext *ctx = user;
    (void)instance;
    ctx->unloads++;
    gasset_Cache_destroy(alloc, ctx->cache);
}

TEST(asset_cache_reentrant, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset__TestReentrantContext ctx = {0};
    ctx.cache = gasset_Cache_create(alloc);
    mewassert("register", gasset_Cache_register(alloc, ctx.cache, "outer", sizeof(int), gasset__test_reentrant_load,
                                                     NULL, gasset__test_reentrant_unload, &ctx));
    ctx.trigger = 1;
    int *main = gasset_Cache_load(alloc, ctx.cache, "outer", "main");
    mewassert("reentrant initial load", main && *main == 42 && ctx.callbacks == 1);
    mewassert("fallback reload succeeds", gasset_Cache_reload(alloc, ctx.cache, "outer", "main"));
    mewassert("instance retained", gasset_Cache_get(ctx.cache, "outer", "main") == main);
    ctx.fail = 1;
    mewassert("fallback reload fails", !gasset_Cache_reload(alloc, ctx.cache, "outer", "main"));
    mewassert("failed entry evicted", !gasset_Cache_get(ctx.cache, "outer", "main"));
    mewassert("dependencies survive", gasset_Cache_get(ctx.cache, "outer", "dep3-7"));
    mewassert("failed initial load", !gasset_Cache_load(alloc, ctx.cache, "outer", "main"));
    ctx.fail = 0;
    mewassert("retry after failure", gasset_Cache_load(alloc, ctx.cache, "outer", "main"));
    gasset_Cache_destroy(alloc, ctx.cache);
})

TEST(asset_cache_reentrant_custom_reload, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gasset__TestReentrantContext ctx = {0};
    ctx.cache = gasset_Cache_create(alloc);
    mewassert("register", gasset_Cache_register(alloc, ctx.cache, "outer", sizeof(int), gasset__test_reentrant_load,
                                                     gasset__test_reentrant_reload, gasset__test_reentrant_unload, &ctx));
    int *main = gasset_Cache_load(alloc, ctx.cache, "outer", "main");
    ctx.fail = 1;
    mewassert("custom reload fails", !gasset_Cache_reload(alloc, ctx.cache, "outer", "main"));
    mewassert("custom failure retains entry", gasset_Cache_get(ctx.cache, "outer", "main") == main);
    ctx.fail = 0;
    mewassert("custom reload succeeds", gasset_Cache_reload(alloc, ctx.cache, "outer", "main"));
    mewassert("custom reload updates instance", *main == 99);
    gasset_Cache_destroy(alloc, ctx.cache);
})

typedef struct gasset__TestFailingAllocator {
    galloc_Allocator backing;
    int fail_realloc;
} gasset__TestFailingAllocator;

static void *gasset__test_failing_alloc(void *data, size_t size) {
    return galloc_alloc(((gasset__TestFailingAllocator *)data)->backing, size);
}
static void gasset__test_failing_free(void *data, void *ptr) {
    galloc_free(((gasset__TestFailingAllocator *)data)->backing, ptr);
}
static void *gasset__test_failing_calloc(void *data, size_t count, size_t size) {
    return galloc_calloc(((gasset__TestFailingAllocator *)data)->backing, count, size);
}
static void *gasset__test_failing_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gasset__TestFailingAllocator *state = data;
    return state->fail_realloc ? NULL : galloc_realloc(state->backing, ptr, old_size, new_size);
}

TEST(asset_cache_reentrant_allocation_failure, {
    static const galloc_FunctionTable gasset__test_table = {gasset__test_failing_alloc, gasset__test_failing_free, gasset__test_failing_calloc, gasset__test_failing_realloc};
    gasset__TestFailingAllocator state = {.backing = galloc_malloc_allocator()};
    galloc_Allocator alloc = {.data = &state, .ftable = &gasset__test_table};
    gasset__TestReentrantContext ctx = {0};
    ctx.cache = gasset_Cache_create(alloc);
    mewassert("register", gasset_Cache_register(alloc, ctx.cache, "outer", sizeof(int), gasset__test_reentrant_load,
                                                     NULL, gasset__test_reentrant_unload, &ctx));
    ctx.trigger = 1;
    ctx.fail_realloc = &state.fail_realloc;
    mewassert("entry growth failure", !gasset_Cache_load(alloc, ctx.cache, "outer", "main"));
    mewassert("loaded instance cleaned up", ctx.unloads == 1);
    mewassert("not cached", !gasset_Cache_get(ctx.cache, "outer", "main"));
    state.fail_realloc = 0;
    ctx.fail_realloc = NULL;
    ctx.trigger = 0;
    mewassert("retry succeeds", gasset_Cache_load(alloc, ctx.cache, "outer", "main"));
    gasset_Cache_destroy(alloc, ctx.cache);
})

mewtest_main();

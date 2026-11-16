#include <glint/config.h>
#include <glint/platform/filesystem.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <string.h>

typedef struct gfs__TestContext {
    galloc_Allocator alloc;
    galloc_FunctionTable fail_table;
    galloc_Allocator failing;
    FILE* file;
    void* data;
    size_t size;
    size_t visits;
    bool fixture;
} gfs__TestContext;

static bool gfs__test_create(gfs__TestContext* ctx);
static bool gfs__test_truncate(gfs__TestContext* ctx);
static void gfs__test_free(void* user);

static void* gfs__test_fail_alloc(void* user, size_t bytes);
static void* gfs__test_fail_calloc(void* user, size_t count, size_t size);
static bool gfs__test_stop_listing(const char* name, void* user);

GUNIT_TEST(native_file_contents, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("read native file", gfs_read(ctx.alloc, "glint-filesystem-test.tmp", &ctx.data, &ctx.size) == GFS_OK);
    gunit_assert("native contents and size", ctx.size == 6 && memcmp(ctx.data, "native", ctx.size) == 0);
    gunit_return();
})

GUNIT_TEST(native_file_exists, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("native file exists", gfs_exists(ctx.alloc, "glint-filesystem-test.tmp"));
    gunit_return();
})

GUNIT_TEST(read_allocation_failure, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    void* data = &ctx.size;
    size_t size = 99;
    gunit_assert(
        "allocation failure reports OOM",
        gfs_read(ctx.failing, "glint-filesystem-test.tmp", &data, &size) == GFS_OOM
    );
    gunit_assert("OOM clears outputs", data == NULL && size == 0);
    gunit_return();
})

GUNIT_TEST(read_invalid_arguments, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    void* data = &ctx.size;
    size_t size = 99;
    gunit_assert("invalid path reports error", gfs_read(ctx.alloc, NULL, &data, &size) == GFS_ERROR);
    gunit_assert("invalid read clears outputs", data == NULL && size == 0);
    gunit_return();
})

GUNIT_TEST(directory_early_stop, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("early stop succeeds", gfs_list(ctx.alloc, ".", gfs__test_stop_listing, &ctx.visits) == GFS_OK);
    gunit_assert("visitor stops after one entry", ctx.visits == 1);
    gunit_return();
})

GUNIT_TEST(directory_unavailable, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert(
        "missing directory is unavailable",
        gfs_list(ctx.alloc, "glint-filesystem-test.tmp/child", gfs__test_stop_listing, &ctx.visits) == GFS_UNAVAILABLE
    );
    gunit_assert("unavailable directory has no entries", ctx.visits == 0);
    gunit_return();
})

GUNIT_TEST(empty_file_owns_allocation, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("truncate filesystem fixture", gfs__test_truncate(&ctx));
    gunit_assert(
        "read empty native file",
        gfs_read(ctx.alloc, "glint-filesystem-test.tmp", &ctx.data, &ctx.size) == GFS_OK
    );
    gunit_assert("empty file still owns allocation", ctx.data != NULL && ctx.size == 0);
    gunit_return();
})

GUNIT_TEST(empty_file_allocation_failure, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("truncate filesystem fixture", gfs__test_truncate(&ctx));
    void* data = &ctx.size;
    size_t size = 99;
    gunit_assert("empty read reports OOM", gfs_read(ctx.failing, "glint-filesystem-test.tmp", &data, &size) == GFS_OOM);
    gunit_assert("empty read OOM clears outputs", data == NULL && size == 0);
    gunit_return();
})

GUNIT_TEST(removed_file_is_unavailable, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("remove native fixture", remove("glint-filesystem-test.tmp") == 0);
    ctx.fixture = false;
    void* data = &ctx.size;
    size_t size = 99;
    gunit_assert(
        "removed file is unavailable",
        gfs_read(ctx.alloc, "glint-filesystem-test.tmp", &data, &size) == GFS_UNAVAILABLE
    );
    gunit_assert("unavailable file clears outputs", data == NULL && size == 0);
    gunit_return();
})

#if GLINT_PLATFORM_WINDOWS

GUNIT_TEST(windows_read_buffer_allocation_failure, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    size_t remaining = 1;
    ctx.failing.data = &remaining;
    void* data = &ctx.size;
    size_t size = 99;
    gunit_assert(
        "read buffer allocation failure reports OOM",
        gfs_read(ctx.failing, "glint-filesystem-test.tmp", &data, &size) == GFS_OOM
    );
    gunit_assert("read OOM clears outputs", data == NULL && size == 0);
    gunit_return();
})

GUNIT_TEST(windows_list_pattern_allocation_failure, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    size_t remaining = 0;
    ctx.failing.data = &remaining;
    gunit_assert(
        "listing allocation failure reports OOM",
        gfs_list(ctx.failing, ".", gfs__test_stop_listing, &ctx.visits) == GFS_OOM
    );
    gunit_assert("failed listing never calls visitor", ctx.visits == 0);
    gunit_return();
})

GUNIT_TEST(windows_list_path_conversion_allocation_failure, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    size_t remaining = 1;
    ctx.failing.data = &remaining;
    gunit_assert(
        "listing allocation failure reports OOM",
        gfs_list(ctx.failing, ".", gfs__test_stop_listing, &ctx.visits) == GFS_OOM
    );
    gunit_assert("failed listing never calls visitor", ctx.visits == 0);
    gunit_return();
})

GUNIT_TEST(windows_list_entry_conversion_allocation_failure, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    size_t remaining = 2;
    ctx.failing.data = &remaining;
    gunit_assert(
        "listing allocation failure reports OOM",
        gfs_list(ctx.failing, ".", gfs__test_stop_listing, &ctx.visits) == GFS_OOM
    );
    gunit_assert("failed listing never calls visitor", ctx.visits == 0);
    gunit_return();
})

GUNIT_TEST(windows_rejects_invalid_utf8, {
    gfs__TestContext ctx = {0};
    gunit_defer(gfs__test_free, &ctx);
    gunit_assert("create filesystem fixture", gfs__test_create(&ctx));
    gunit_assert("invalid UTF-8 reports non-OOM error", gfs_read(ctx.alloc, "\xff", &ctx.data, &ctx.size) == GFS_ERROR);
    gunit_return();
})

#endif
gunit_main();

static void* gfs__test_fail_alloc(void* user, size_t bytes) {
    size_t* remaining = user;
    if (remaining == NULL || *remaining == 0) {
        return NULL;
    }

    *remaining -= 1;
    return galloc_alloc(galloc_malloc_allocator(), bytes);
}

static void* gfs__test_fail_calloc(void* user, size_t count, size_t size) {
    size_t* remaining = user;
    if (remaining == NULL || *remaining == 0) {
        return NULL;
    }

    *remaining -= 1;
    return galloc_calloc(galloc_malloc_allocator(), count, size);
}

static bool gfs__test_stop_listing(const char* name, void* user) {
    if (!gunit_check("directory listing omits dot entries", strcmp(name, ".") != 0 && strcmp(name, "..") != 0)) {
        return false;
    }
    size_t* visits = user;
    *visits += 1;
    return false;
}

static bool gfs__test_create(gfs__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    ctx->fail_table = *ctx->alloc.ftable;
    ctx->fail_table.alloc = gfs__test_fail_alloc;
    ctx->fail_table.calloc = gfs__test_fail_calloc;
    ctx->failing = (galloc_Allocator) {.ftable = &ctx->fail_table};
    ctx->file = fopen("glint-filesystem-test.tmp", "wb");
    if (!gunit_check("create native fixture", ctx->file != NULL)) {
        return false;
    }
    ctx->fixture = true;
    if (!gunit_check("write native fixture", fwrite("native", 1, 6, ctx->file) == 6)) {
        return false;
    }
    int closed = fclose(ctx->file);
    ctx->file = NULL;
    if (!gunit_check("close native fixture", closed == 0)) {
        return false;
    }
    return true;
}

static bool gfs__test_truncate(gfs__TestContext* ctx) {
    ctx->file = fopen("glint-filesystem-test.tmp", "wb");
    if (!gunit_check("truncate native fixture", ctx->file != NULL)) {
        return false;
    }
    int closed = fclose(ctx->file);
    ctx->file = NULL;
    if (!gunit_check("close empty native fixture", closed == 0)) {
        return false;
    }
    return true;
}

static void gfs__test_free(void* user) {
    gfs__TestContext* ctx = user;
    if (ctx->file != NULL) {
        fclose(ctx->file);
    }
    galloc_free(ctx->alloc, ctx->data);
    if (ctx->fixture) {
        remove("glint-filesystem-test.tmp");
    }
}

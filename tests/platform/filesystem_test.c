#include <glint/config.h>
#include <glint/platform/filesystem.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <string.h>

typedef struct gfs_test__Context {
    galloc_Allocator alloc;
    galloc_FunctionTable fail_table;
    galloc_Allocator failing;
    size_t allocations_before_failure;
    FILE* file;
    void* data;
    size_t visits;
    bool owns_file;
} gfs_test__Context;

static const char gfs_test__path[] = "glint-filesystem-test.tmp";

static bool gfs_test__create(gfs_test__Context* ctx, const char* contents);
static void gfs_test__free(void* user);
static void* gfs_test__fail_alloc(void* user, size_t bytes);
static void* gfs_test__fail_calloc(void* user, size_t count, size_t size);
static bool gfs_test__stop_listing(const char* name, void* user);
static bool gfs_test__read_fails(galloc_Allocator alloc, const char* path, gfs_Result expected);
#if GLINT_PLATFORM_WINDOWS
static bool gfs_test__list_allocation_fails(gfs_test__Context* ctx);
#endif

GUNIT_TEST(filesystem_read,
    gfs_test__Context ctx = {0};
    const char contents[] = "native";

    GUNIT_SETUP({
        gunit_defer(gfs_test__free, &ctx);
        gunit_assert("create nonempty native file", gfs_test__create(&ctx, contents));
    })

    GUNIT_SECTION("an existing file is reported as present", {
        gunit_assert("native file exists", gfs_exists(ctx.alloc, gfs_test__path));
    })

    GUNIT_SECTION("reading an existing file returns its contents", {
        size_t size = 0;
        gunit_assert("read native file", gfs_read(ctx.alloc, gfs_test__path, &ctx.data, &size) == GFS_OK);

        gunit_assert("returned byte count excludes terminator", size == sizeof(contents) - 1);
        gunit_assert("native file contents match", ctx.data != NULL && memcmp(ctx.data, contents, size) == 0);
    })

    GUNIT_SECTION("reading a removed file reports unavailable", {
        gunit_assert("remove native fixture", remove(gfs_test__path) == 0);
        ctx.owns_file = false;
        gunit_assert("file is unavailable", gfs_test__read_fails(ctx.alloc, gfs_test__path, GFS_UNAVAILABLE));
    })

    GUNIT_SECTION("reading a null path reports an error", {
        gunit_assert("null path is invalid", gfs_test__read_fails(ctx.alloc, NULL, GFS_ERROR));
    })

    GUNIT_SECTION("failure to allocate reports out of memory", {
        gunit_assert("allocation is denied", gfs_test__read_fails(ctx.failing, gfs_test__path, GFS_OOM));
    })
)

GUNIT_TEST(filesystem_read_empty_file,
    gfs_test__Context ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gfs_test__free, &ctx);
        gunit_assert("create empty native file", gfs_test__create(&ctx, ""));
    })

    GUNIT_SECTION("reading returns an owned allocation with zero bytes", {
        size_t size = 99;
        gunit_assert("read empty native file", gfs_read(ctx.alloc, gfs_test__path, &ctx.data, &size) == GFS_OK);

        gunit_assert("empty contents still own an allocation", ctx.data != NULL);
        gunit_assert("byte count is zero", size == 0);
    })

    GUNIT_SECTION("failure to allocate reports out of memory", {
        gunit_assert("allocation is denied", gfs_test__read_fails(ctx.failing, gfs_test__path, GFS_OOM));
    })
)

GUNIT_TEST(filesystem_list,
    gfs_test__Context ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gfs_test__free, &ctx);
        gunit_assert("create an entry in the working directory", gfs_test__create(&ctx, "native"));
    })

    GUNIT_SECTION("a visitor can stop after one entry", {
        gunit_assert("early stop succeeds", gfs_list(ctx.alloc, ".", gfs_test__stop_listing, &ctx.visits) == GFS_OK);
        gunit_assert("visitor stops after one entry", ctx.visits == 1);
    })

    GUNIT_SECTION("a missing directory reports unavailable", {
        const char* missing_directory = "glint-filesystem-test.tmp/child";
        gunit_assert(
            "directory is unavailable",
            gfs_list(ctx.alloc, missing_directory, gfs_test__stop_listing, &ctx.visits) == GFS_UNAVAILABLE
        );
        gunit_assert("visitor receives no entries", ctx.visits == 0);
    })
)

#if GLINT_PLATFORM_WINDOWS
GUNIT_TEST(filesystem_read_windows,
    gfs_test__Context ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gfs_test__free, &ctx);
        gunit_assert("create nonempty native file", gfs_test__create(&ctx, "native"));
    })

    GUNIT_SECTION("failure to allocate the read buffer reports out of memory", {
        // Let the UTF-16 path allocation succeed first.
        ctx.allocations_before_failure = 1;
        gunit_assert("buffer allocation is denied", gfs_test__read_fails(ctx.failing, gfs_test__path, GFS_OOM));
    })

    GUNIT_SECTION("invalid UTF-8 reports a path error", {
        gunit_assert("path encoding is invalid", gfs_test__read_fails(ctx.alloc, "\xff", GFS_ERROR));
    })
)

GUNIT_TEST(filesystem_list_windows,
    gfs_test__Context ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gfs_test__free, &ctx);
        gunit_assert("create an entry in the working directory", gfs_test__create(&ctx, "native"));
    })

    GUNIT_SECTION("failure to allocate a search pattern reports out of memory", {
        gunit_assert("search pattern allocation is denied", gfs_test__list_allocation_fails(&ctx));
    })

    GUNIT_SECTION("failure to convert a path reports out of memory", {
        ctx.allocations_before_failure = 1;
        gunit_assert("path conversion allocation is denied", gfs_test__list_allocation_fails(&ctx));
    })

    GUNIT_SECTION("failure to convert an entry name reports out of memory", {
        ctx.allocations_before_failure = 2;
        gunit_assert("entry name conversion allocation is denied", gfs_test__list_allocation_fails(&ctx));
    })
)
#endif

gunit_main();

static void* gfs_test__fail_alloc(void* user, size_t bytes) {
    size_t* remaining = user;
    if (remaining == NULL || *remaining == 0) {
        return NULL;
    }

    *remaining -= 1;
    return galloc_alloc(galloc_malloc_allocator(), bytes);
}

static void* gfs_test__fail_calloc(void* user, size_t count, size_t size) {
    size_t* remaining = user;
    if (remaining == NULL || *remaining == 0) {
        return NULL;
    }

    *remaining -= 1;
    return galloc_calloc(galloc_malloc_allocator(), count, size);
}

static bool gfs_test__stop_listing(const char* name, void* user) {
    if (!gunit_check("directory listing omits dot entries", strcmp(name, ".") != 0 && strcmp(name, "..") != 0)) {
        return false;
    }
    size_t* visits = user;
    *visits += 1;
    return false;
}

static bool gfs_test__create(gfs_test__Context* ctx, const char* contents) {
    ctx->alloc = galloc_malloc_allocator();
    ctx->fail_table = *ctx->alloc.ftable;
    ctx->fail_table.alloc = gfs_test__fail_alloc;
    ctx->fail_table.calloc = gfs_test__fail_calloc;
    ctx->failing = (galloc_Allocator) {.data = &ctx->allocations_before_failure, .ftable = &ctx->fail_table};

    ctx->file = fopen(gfs_test__path, "wb");
    if (!gunit_check("create native fixture", ctx->file != NULL)) {
        return false;
    }

    ctx->owns_file = true;
    const size_t length = strlen(contents);
    if (!gunit_check("write native fixture", fwrite(contents, 1, length, ctx->file) == length)) {
        return false;
    }

    const int closed = fclose(ctx->file);
    ctx->file = NULL;
    return gunit_check("close native fixture", closed == 0);
}

static void gfs_test__free(void* user) {
    gfs_test__Context* ctx = user;
    if (ctx->file != NULL) {
        fclose(ctx->file);
    }
    galloc_free(ctx->alloc, ctx->data);
    if (ctx->owns_file) {
        remove(gfs_test__path);
    }
}

static bool gfs_test__read_fails(galloc_Allocator alloc, const char* path, gfs_Result expected) {
    size_t size = 99;
    void* data = &size;
    const gfs_Result result = gfs_read(alloc, path, &data, &size);
    const bool outputs_cleared = data == NULL && size == 0;
    if (result == GFS_OK && data != &size) {
        galloc_free(alloc, data);
    }

    if (!gunit_check("read returns expected error", result == expected)) {
        return false;
    }
    return gunit_check("failed read clears both outputs", outputs_cleared);
}

#if GLINT_PLATFORM_WINDOWS
static bool gfs_test__list_allocation_fails(gfs_test__Context* ctx) {
    const gfs_Result result = gfs_list(ctx->failing, ".", gfs_test__stop_listing, &ctx->visits);
    if (!gunit_check("listing returns out of memory", result == GFS_OOM)) {
        return false;
    }
    return gunit_check("failed listing never calls the visitor", ctx->visits == 0);
}
#endif

#include <glint/unit/unit.h>
#include <glint/vfs/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gvfs__TestContext {
    galloc_Allocator alloc;
    gvfs_Vfs* fs;
    void* data;
    char* text;
    char* name;
    char* cwd;
    FILE* file;
    const char* fixture;
    bool destroyed;
} gvfs__TestContext;

typedef struct gvfs__TestMountedFile {
    const char* path;
    const char* text;
} gvfs__TestMountedFile;

static bool gvfs__test_mounted_exists(galloc_Allocator alloc, void* user, const char* path);
static void* gvfs__test_mounted_read(galloc_Allocator alloc, void* user, const char* path, size_t* size);
static void gvfs__test_mounted_destroy(galloc_Allocator alloc, void* user);

static bool gvfs__test_create(gvfs__TestContext* ctx);
static bool gvfs__test_mount_assets(gvfs__TestContext* ctx);
static bool gvfs__test_mount_virtual(gvfs__TestContext* ctx);
static bool gvfs__test_mount_fallback(gvfs__TestContext* ctx);
static bool gvfs__test_fixture(gvfs__TestContext* ctx, const char* path, const char* text);
static void gvfs__test_free(void* user);

static void* gvfs__test_fail_alloc(void* user, size_t bytes);
static void* gvfs__test_fail_calloc(void* user, size_t count, size_t size);
static bool gvfs__test_find_game(const char* path, void* user);
static bool gvfs__test_stop_listing(const char* path, void* user);
static bool gvfs__test_collect_point(const char* point, void* user);
static bool gvfs__test_virtual_exists(galloc_Allocator alloc, void* ctx, const char* path);
static void* gvfs__test_virtual_read(galloc_Allocator alloc, void* ctx, const char* path, size_t* size);
static bool gvfs__test_virtual_list(
    galloc_Allocator alloc,
    void* ctx,
    const char* path,
    gvfs_EntryFn visit,
    void* user
);
static void gvfs__test_virtual_destroy(galloc_Allocator alloc, void* ctx);

GUNIT_TEST(virtual_files_exist, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount virtual fixture", gvfs__test_mount_virtual(&ctx));
    gunit_assert("virtual file exists", gvfs_exists(ctx.alloc, ctx.fs, "/assets/game.lua"));
    gunit_return();
})

GUNIT_TEST(virtual_mount_precedes_native, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount virtual fixture", gvfs__test_mount_virtual(&ctx));
    gunit_assert("native fallback", gvfs_mount(ctx.alloc, ctx.fs, "/assets", gunit_argv[1]));
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/assets/game.lua");
    gunit_assert("first backend takes precedence", ctx.text != NULL && strcmp(ctx.text, "virtual") == 0);
    gunit_return();
})

GUNIT_TEST(more_specific_mount_precedes_earlier_parent_mount, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gvfs__TestMountedFile parent = {.path = "deep/game.lua", .text = "parent"};
    gvfs__TestMountedFile child = {.path = "game.lua", .text = "child"};
    gvfs_Filesystem backend = {
        .context = &parent,
        .exists = gvfs__test_mounted_exists,
        .read = gvfs__test_mounted_read,
        .list = gvfs__test_virtual_list,
        .destroy = gvfs__test_mounted_destroy
    };
    gunit_assert("mount parent first", gvfs_mount_filesystem(ctx.alloc, ctx.fs, "/assets", backend));
    // Both backends can serve the requested file. The child must win by
    // specificity even though the parent was mounted first.
    backend.context = &child;
    gunit_assert("mount child second", gvfs_mount_filesystem(ctx.alloc, ctx.fs, "/assets/deep", backend));
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/assets/deep/game.lua");
    gunit_assert("most specific mount supplies file", ctx.text != NULL && strcmp(ctx.text, "child") == 0);
    gunit_return();
})

GUNIT_TEST(virtual_file_has_no_native_path, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount virtual fixture", gvfs__test_mount_virtual(&ctx));
    ctx.name = gvfs_real_path(ctx.alloc, ctx.fs, "/assets/game.lua");
    gunit_assert("virtual file has no native path", ctx.name == NULL);
    gunit_return();
})

GUNIT_TEST(virtual_directory_listing, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount virtual fixture", gvfs__test_mount_virtual(&ctx));
    bool found = false;
    gunit_assert("virtual directory listed", gvfs_list(ctx.alloc, ctx.fs, "/assets", gvfs__test_find_game, &found));
    gunit_assert("virtual script listed", found);
    gunit_return();
})

GUNIT_TEST(virtual_backend_destroyed, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount virtual fixture", gvfs__test_mount_virtual(&ctx));
    gvfs_destroy(ctx.alloc, ctx.fs);
    ctx.fs = NULL;
    gunit_assert("backend destroy callback runs", ctx.destroyed);
    gunit_return();
})

GUNIT_TEST(native_file_read, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    size_t size = 99;
    ctx.data = gvfs_read(ctx.alloc, ctx.fs, "/assets/game.lua", &size);
    gunit_assert("mounted file read", ctx.data != NULL && size > 0);
    gunit_return();
})

GUNIT_TEST(native_file_real_path, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    ctx.name = gvfs_real_path(ctx.alloc, ctx.fs, "/assets/game.lua");
    gunit_assert("native file resolves", ctx.name != NULL);
    gunit_return();
})

GUNIT_TEST(traversal_rejected, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    size_t size = 99;
    ctx.data = gvfs_read(ctx.alloc, ctx.fs, "/assets/../basic/game.lua", &size);
    gunit_assert("traversal rejected and size cleared", ctx.data == NULL && size == 0);
    gunit_assert("traversal does not exist", !gvfs_exists(ctx.alloc, ctx.fs, "/assets/../basic/game.lua"));
    gunit_return();
})

GUNIT_TEST(mount_prefix_boundary, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    ctx.name = gvfs_real_path(ctx.alloc, ctx.fs, "/assets2/game.lua");
    gunit_assert("mount prefix ends at boundary", ctx.name == NULL);
    gunit_return();
})

GUNIT_TEST(relative_mount_rejected, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("relative mount rejected", !gvfs_mount(ctx.alloc, ctx.fs, "relative", gunit_argv[1]));
    gunit_return();
})

GUNIT_TEST(native_directory_listing, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    bool found = false;
    gunit_assert("native directory enumerated", gvfs_list(ctx.alloc, ctx.fs, "/assets", gvfs__test_find_game, &found));
    gunit_assert("directory contains script", found);
    gunit_return();
})

GUNIT_TEST(native_read_falls_back, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount fallback fixtures", gvfs__test_mount_fallback(&ctx));
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/assets/game.lua");
    gunit_assert("unavailable mount preserves fallback read", ctx.text != NULL && ctx.text[0] != '\0');
    gunit_return();
})

GUNIT_TEST(native_listing_falls_back, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount fallback fixtures", gvfs__test_mount_fallback(&ctx));
    bool found = false;
    gunit_assert(
        "unavailable mount preserves fallback listing",
        gvfs_list(ctx.alloc, ctx.fs, "/assets", gvfs__test_find_game, &found)
    );
    gunit_assert("fallback lists script", found);
    gunit_return();
})

GUNIT_TEST(native_listing_stops_across_mounts, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount fallback fixtures", gvfs__test_mount_fallback(&ctx));
    size_t visits = 0;
    gunit_assert("early stop succeeds", gvfs_list(ctx.alloc, ctx.fs, "/assets", gvfs__test_stop_listing, &visits));
    gunit_assert("stop skips remaining entries and mounts", visits == 1);
    gunit_return();
})

GUNIT_TEST(read_string_is_nul_terminated, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    size_t size = 0;
    ctx.data = gvfs_read(ctx.alloc, ctx.fs, "/assets/game.lua", &size);
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/assets/game.lua");
    gunit_assert("raw and string reads succeed", ctx.data != NULL && ctx.text != NULL);
    gunit_assert("string length matches byte count", strlen(ctx.text) == size);
    gunit_assert(
        "string preserves bytes and terminates",
        memcmp(ctx.data, ctx.text, size) == 0 && ctx.text[size] == '\0'
    );
    gunit_return();
})

GUNIT_TEST(empty_file_is_empty_string, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("write VFS fixture", gvfs__test_fixture(&ctx, "glint-vfs-empty.txt", ""));
    gunit_assert("mount fixture directory", gvfs_mount(ctx.alloc, ctx.fs, "/tmp", "."));
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/tmp/glint-vfs-empty.txt");
    gunit_assert("empty string owns allocation", ctx.text != NULL && ctx.text[0] == '\0');
    gunit_assert("empty file exists", gvfs_exists(ctx.alloc, ctx.fs, "/tmp/glint-vfs-empty.txt"));
    gunit_return();
})

GUNIT_TEST(missing_file_does_not_exist, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount asset fixture", gvfs__test_mount_assets(&ctx));
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/assets/missing.lua");
    gunit_assert("missing file does not read", ctx.text == NULL);
    gunit_assert("missing file does not exist", !gvfs_exists(ctx.alloc, ctx.fs, "/assets/missing.lua"));
    gunit_assert("existing file exists", gvfs_exists(ctx.alloc, ctx.fs, "/assets/game.lua"));
    gunit_return();
})

GUNIT_TEST(empty_vfs_has_no_mount_points, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    char* points[3] = {0};
    gunit_assert("empty VFS has no points", !gvfs_points(ctx.fs, gvfs__test_collect_point, points));
    gunit_return();
})

GUNIT_TEST(mount_points_are_unique_and_ordered, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    gunit_assert("mount assets", gvfs_mount(ctx.alloc, ctx.fs, "/assets", gunit_argv[1]));
    gunit_assert("mount extra", gvfs_mount(ctx.alloc, ctx.fs, "/extra", "."));
    gunit_assert("repeat mount", gvfs_mount(ctx.alloc, ctx.fs, "/assets", gunit_argv[1]));
    char* points[3] = {0};
    gunit_assert("enumerate mount points", gvfs_points(ctx.fs, gvfs__test_collect_point, points));
    gunit_assert("mounts occur exactly once", points[0] != NULL && points[1] != NULL && points[2] == NULL);
    gunit_assert("points sorted by specificity", strcmp(points[0], "/assets") == 0 && strcmp(points[1], "/extra") == 0);
    gunit_return();
})

GUNIT_TEST(working_directory_allocation_failure, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    galloc_FunctionTable table = *ctx.alloc.ftable;
    table.alloc = gvfs__test_fail_alloc;
    table.calloc = gvfs__test_fail_calloc;
    galloc_Allocator failing = {.ftable = &table};
    ctx.cwd = gvfs_get_working_directory(failing);
    gunit_assert("working directory handles allocation failure", ctx.cwd == NULL);
    gunit_return();
})

GUNIT_TEST(working_directory_mount, {
    gvfs__TestContext ctx = {0};
    gunit_defer(gvfs__test_free, &ctx);
    gunit_assert("create VFS fixture", gvfs__test_create(&ctx));
    ctx.cwd = gvfs_get_working_directory(ctx.alloc);
    gunit_assert("working directory available", ctx.cwd != NULL && ctx.cwd[0] != '\0');
    gunit_assert("write VFS fixture", gvfs__test_fixture(&ctx, "glint-vfs-cwd-test.txt", "cwd"));
    gunit_assert("mount working directory", gvfs_mount(ctx.alloc, ctx.fs, "/", ctx.cwd));
    ctx.text = gvfs_read_str(ctx.alloc, ctx.fs, "/glint-vfs-cwd-test.txt");
    gunit_assert("working directory mount resolves fixture", ctx.text != NULL && strcmp(ctx.text, "cwd") == 0);
    gunit_return();
})
gunit_main();

static void* gvfs__test_fail_alloc(void* user, size_t bytes) {
    (void)user;
    (void)bytes;
    return NULL;
}

static void* gvfs__test_fail_calloc(void* user, size_t count, size_t size) {
    (void)user;
    (void)count;
    (void)size;
    return NULL;
}

static bool gvfs__test_find_game(const char* path, void* user) {
    if (strcmp(path, "/assets/game.lua") == 0) {
        *(bool*)user = true;
    }

    return true;
}

static bool gvfs__test_stop_listing(const char* path, void* user) {
    if (!gunit_check("entry uses the virtual path", strncmp(path, "/assets/", 8) == 0)) {
        return false;
    }
    size_t* visits = user;
    *visits += 1;
    return false;
}

static bool gvfs__test_collect_point(const char* point, void* user) {
    char** points = user;
    if (points[0] == NULL) {
        points[0] = (char*)point;
    } else if (points[1] == NULL) {
        points[1] = (char*)point;
    } else if (points[2] == NULL) {
        points[2] = (char*)point;
    }

    return points[2] == NULL;
}

static bool gvfs__test_mounted_exists(galloc_Allocator alloc, void* user, const char* path) {
    (void)alloc;
    const gvfs__TestMountedFile* file = user;
    return strcmp(path, file->path) == 0;
}

static void* gvfs__test_mounted_read(galloc_Allocator alloc, void* user, const char* path, size_t* size) {
    const gvfs__TestMountedFile* file = user;
    if (!gvfs__test_mounted_exists(alloc, user, path)) {
        return NULL;
    }
    size_t length = strlen(file->text);
    void* data = galloc_alloc(alloc, length);
    if (data == NULL) {
        return NULL;
    }
    memcpy(data, file->text, length);
    if (size != NULL) {
        *size = length;
    }
    return data;
}

static void gvfs__test_mounted_destroy(galloc_Allocator alloc, void* user) {
    (void)alloc;
    (void)user;
}

static bool gvfs__test_virtual_exists(galloc_Allocator alloc, void* ctx, const char* path) {
    (void)alloc;
    (void)ctx;
    return strcmp(path, "game.lua") == 0;
}

static void* gvfs__test_virtual_read(galloc_Allocator alloc, void* ctx, const char* path, size_t* size) {
    (void)ctx;
    if (strcmp(path, "game.lua") != 0) {
        return NULL;
    }

    char* data = galloc_alloc(alloc, 7);
    if (data == NULL) {
        return NULL;
    }

    memcpy(data, "virtual", 7);
    if (size != NULL) {
        *size = 7;
    }

    return data;
}

static bool gvfs__test_virtual_list(
    galloc_Allocator alloc,
    void* ctx,
    const char* path,
    gvfs_EntryFn visit,
    void* user
) {
    (void)alloc;
    (void)ctx;
    if (*path != 0) {
        return false;
    }

    (void)visit("game.lua", user);
    return true;
}

static void gvfs__test_virtual_destroy(galloc_Allocator alloc, void* ctx) {
    (void)alloc;
    *(bool*)ctx = true;
}

static bool gvfs__test_create(gvfs__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    ctx->fs = gvfs_create(ctx->alloc);
    if (!gunit_check("create VFS", ctx->fs != NULL)) {
        return false;
    }
    if (!gunit_check("asset root argument supplied", gunit_argc == 2)) {
        return false;
    }
    return true;
}

static bool gvfs__test_mount_assets(gvfs__TestContext* ctx) {
    if (!gunit_check("mount asset root", gvfs_mount(ctx->alloc, ctx->fs, "/assets", gunit_argv[1]))) {
        return false;
    }
    return true;
}

static bool gvfs__test_mount_virtual(gvfs__TestContext* ctx) {
    gvfs_Filesystem backend = {
        .context = &ctx->destroyed,
        .exists = gvfs__test_virtual_exists,
        .read = gvfs__test_virtual_read,
        .list = gvfs__test_virtual_list,
        .destroy = gvfs__test_virtual_destroy
    };
    if (!gunit_check("mount virtual backend", gvfs_mount_filesystem(ctx->alloc, ctx->fs, "/assets", backend))) {
        return false;
    }
    return true;
}

static bool gvfs__test_mount_fallback(gvfs__TestContext* ctx) {
    if (!gunit_check(
            "mount unavailable directory",
            gvfs_mount(ctx->alloc, ctx->fs, "/assets", "glint-vfs-missing-directory")
        )) {
        return false;
    }
    if (!gvfs__test_mount_assets(ctx)) {
        return false;
    }
    if (!gvfs__test_mount_assets(ctx)) {
        return false;
    }
    return true;
}

static bool gvfs__test_fixture(gvfs__TestContext* ctx, const char* path, const char* text) {
    ctx->file = fopen(path, "wb");
    if (!gunit_check("create fixture", ctx->file != NULL)) {
        return false;
    }
    ctx->fixture = path;
    if (!gunit_check("write fixture", fputs(text, ctx->file) >= 0)) {
        return false;
    }
    int closed = fclose(ctx->file);
    ctx->file = NULL;
    if (!gunit_check("close fixture", closed == 0)) {
        return false;
    }
    return true;
}

static void gvfs__test_free(void* user) {
    gvfs__TestContext* ctx = user;
    galloc_free(ctx->alloc, ctx->data);
    galloc_free(ctx->alloc, ctx->text);
    galloc_free(ctx->alloc, ctx->name);
    galloc_free(ctx->alloc, ctx->cwd);
    if (ctx->fs != NULL) {
        gvfs_destroy(ctx->alloc, ctx->fs);
    }
    if (ctx->file != NULL) {
        fclose(ctx->file);
    }
    if (ctx->fixture != NULL) {
        remove(ctx->fixture);
    }
}

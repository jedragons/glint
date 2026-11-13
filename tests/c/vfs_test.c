#include <glint/vfs.h>
#include <mewunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *gvfs__test_fail_alloc(void *user, size_t bytes) {
    (void)user;
    (void)bytes;
    return NULL;
}

static int gvfs__test_find_game(const char *path, void *user) {
    if (!strcmp(path, "/assets/game.lua")) {
        *(int *)user = 1;
    }
    return 1;
}

static int gvfs__test_collect_point(const char *point, void *user) {
    char **points = user;
    if (!points[0]) {
        points[0] = (char *)point;
    } else if (!points[1]) {
        points[1] = (char *)point;
    } else if (!points[2]) {
        points[2] = (char *)point;
    }
    return points[2] == NULL;
}

TEST(vfs_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gvfs_Vfs *fs = gvfs_create(alloc);
    size_t size = 99;
    char *data, *name;
    mewassert("Test expectation failed", mew_test_argc == 2 && fs);
    mewassert("Test expectation failed", gvfs_mount(alloc, fs, "/assets", mew_test_argv[1]));
    data = gvfs_read(alloc, fs, "/assets/game.lua", &size);
    mewassert("Test expectation failed", data && size > 0);
    galloc_free(alloc, data);
    name = gvfs_real_path(alloc, fs, "/assets/game.lua");
    mewassert("Test expectation failed", name);
    galloc_free(alloc, name);
    mewassert("Test expectation failed", !gvfs_read(alloc, fs, "/assets/../basic/game.lua", &size) && size == 0);
    mewassert("Test expectation failed", !gvfs_real_path(alloc, fs, "/assets2/game.lua"));
    mewassert("Test expectation failed", !gvfs_mount(alloc, fs, "relative", mew_test_argv[1]));
    int found = 0;
    mewassert("directory enumerated", gvfs_list(alloc, fs, "/assets", gvfs__test_find_game, &found));
    mewassert("directory contains game.lua", found);
    gvfs_destroy(alloc, fs);
})

TEST(vfs_str_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gvfs_Vfs *fs = gvfs_create(alloc);
    mewassert("Test expectation failed", fs && gvfs_mount(alloc, fs, "/assets", mew_test_argv[1]));
    /* A shadowing newer mount holds an empty file next to real assets. */
    FILE *empty = fopen("glint-vfs-empty.txt", "wb");
    mewassert("Test expectation failed", empty && fclose(empty) == 0);
    mewassert("Test expectation failed", gvfs_mount(alloc, fs, "/tmp", "."));

    size_t size = 0;
    char *raw = gvfs_read(alloc, fs, "/assets/game.lua", &size);
    char *text = gvfs_read_str(alloc, fs, "/assets/game.lua");
    mewassert("read_str returns NUL-terminated bytes", raw && text && strlen(text) == size);
    mewassert("read_str preserves content", memcmp(raw, text, size) == 0 && text[size] == '\0');
    galloc_free(alloc, raw);
    galloc_free(alloc, text);

    text = gvfs_read_str(alloc, fs, "/tmp/glint-vfs-empty.txt");
    mewassert("empty file reads as an empty string", text && *text == '\0');
    galloc_free(alloc, text);
    mewassert("missing files do not read", !gvfs_read_str(alloc, fs, "/assets/missing.lua"));

    mewassert("existing files resolve", gvfs_exists(alloc, fs, "/assets/game.lua"));
    mewassert("empty files exist", gvfs_exists(alloc, fs, "/tmp/glint-vfs-empty.txt"));
    mewassert("missing files do not exist", !gvfs_exists(alloc, fs, "/assets/missing.lua"));
    mewassert("invalid paths do not exist", !gvfs_exists(alloc, fs, "/assets/../basic/game.lua"));
    gvfs_destroy(alloc, fs);
})

TEST(vfs_points_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gvfs_Vfs *fs = gvfs_create(alloc);
    mewassert("Test expectation failed", fs);
    char *points[3] = {NULL, NULL, NULL};
    mewassert("empty vfs has no points", !gvfs_points(fs, gvfs__test_collect_point, points));

    mewassert("Test expectation failed", gvfs_mount(alloc, fs, "/assets", mew_test_argv[1]));
    mewassert("Test expectation failed", gvfs_mount(alloc, fs, "/extra", "."));
    /* Mounting an already used point again makes it the newest occurrence. */
    mewassert("Test expectation failed", gvfs_mount(alloc, fs, "/assets", mew_test_argv[1]));
    mewassert("points enumerated newest first", gvfs_points(fs, gvfs__test_collect_point, points));
    mewassert("each point is listed once", points[2] == NULL);
    mewassert("newest mount comes first", strcmp(points[0], "/assets") == 0);
    mewassert("older mount comes last", strcmp(points[1], "/extra") == 0);
    gvfs_destroy(alloc, fs);
})

TEST(vfs_working_directory_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    const galloc_FunctionTable fail_table = {.alloc = gvfs__test_fail_alloc};
    galloc_Allocator failing = {.data = NULL, .ftable = &fail_table};
    mewassert("working directory handles allocation failure", !gvfs_get_working_directory(failing));
    char *cwd = gvfs_get_working_directory(alloc);
    mewassert("working directory is available", cwd && *cwd);
    FILE *file = fopen("glint-vfs-cwd-test.txt", "wb");
    mewassert("create cwd fixture", file && fputs("cwd", file) >= 0 && fclose(file) == 0);
    gvfs_Vfs *fs = gvfs_create(alloc);
    mewassert("mount working directory", fs && gvfs_mount(alloc, fs, "/", cwd));
    char *text = gvfs_read_str(alloc, fs, "/glint-vfs-cwd-test.txt");
    mewassert("working directory mount resolves fixture", text && !strcmp(text, "cwd"));
    galloc_free(alloc, text);
    gvfs_destroy(alloc, fs);
    galloc_free(alloc, cwd);
    mewassert("remove cwd fixture", remove("glint-vfs-cwd-test.txt") == 0);
})

mewtest_main();

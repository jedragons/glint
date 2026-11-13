#include <glint/assets/file_watcher.h>

#include <mewunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gwatch__TestAllocator {
    size_t live;
    size_t calls;
    size_t fail_call;
} gwatch__TestAllocator;

static void *gwatch__test_test_alloc(void *data, size_t bytes) {
    gwatch__TestAllocator *state = data;
    if (++state->calls == state->fail_call) {
        return NULL;
    }
    void *ptr = malloc(bytes);
    state->live += ptr != NULL;
    return ptr;
}

static void gwatch__test_test_free(void *data, void *ptr) {
    gwatch__TestAllocator *state = data;
    if (ptr) {
        mewrequire("Test expectation failed", state->live > 0);
        state->live--;
        free(ptr);
    }
}

static void *gwatch__test_test_calloc(void *data, size_t count, size_t bytes) {
    gwatch__TestAllocator *state = data;
    if (++state->calls == state->fail_call) {
        return NULL;
    }
    void *ptr = calloc(count, bytes);
    state->live += ptr != NULL;
    return ptr;
}

static void *gwatch__test_test_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gwatch__TestAllocator *state = data;
    (void)old_size;
    if (++state->calls == state->fail_call) {
        return NULL;
    }
    int is_new = ptr == NULL;
    void *result = realloc(ptr, new_size);
    state->live += result != NULL && is_new;
    return result;
}

static const galloc_FunctionTable gwatch__test_table = {gwatch__test_test_alloc, gwatch__test_test_free, gwatch__test_test_calloc, gwatch__test_test_realloc};

static void gwatch__test_on_change(const char *path, void *user) {
    int *count = user;
    mewrequire("Test expectation failed", strcmp(path, "glint-watcher-test.tmp") == 0);
    ++*count;
}

static void gwatch__test_write_file(const char *contents) {
    FILE *file = fopen("glint-watcher-test.tmp", "wb");
    mewassert("Test expectation failed", file);
    mewassert("Test expectation failed", fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    mewassert("Test expectation failed", fclose(file) == 0);
}

TEST(file_watcher_test, {
    gwatch__TestAllocator state = {0};
    galloc_Allocator alloc = {.data = &state, .ftable = &gwatch__test_table};
    gwatch_Watcher *watcher = gwatch_create(alloc);
    int count = 0;
    mewassert("Test expectation failed", watcher);
    remove("glint-watcher-test.tmp");
    mewassert("Test expectation failed", gwatch_add(alloc, watcher, "glint-watcher-test.tmp"));
    size_t calls = state.calls;
    mewassert("Test expectation failed", gwatch_add(alloc, watcher, "glint-watcher-test.tmp"));
    mewassert("Test expectation failed", state.calls == calls);
    gwatch_poll(watcher, gwatch__test_on_change, &count);
    mewassert("Test expectation failed", count == 0 && state.calls == calls);
    gwatch__test_write_file("first");
    gwatch_poll(watcher, gwatch__test_on_change, &count);
    mewassert("Test expectation failed", count == 1 && state.calls == calls);
    gwatch_poll(watcher, gwatch__test_on_change, &count);
    mewassert("Test expectation failed", count == 1);
    gwatch__test_write_file("longer contents");
    gwatch_poll(watcher, gwatch__test_on_change, &count);
    mewassert("Test expectation failed", count == 2);
    mewassert("Test expectation failed", remove("glint-watcher-test.tmp") == 0);
    gwatch_poll(watcher, gwatch__test_on_change, &count);
    mewassert("Test expectation failed", count == 3);
    gwatch__test_write_file("restored");
    gwatch_poll(watcher, gwatch__test_on_change, &count);
    mewassert("Test expectation failed", count == 4);
    mewassert("Test expectation failed", remove("glint-watcher-test.tmp") == 0);
    gwatch_destroy(alloc, watcher);
    mewassert("Test expectation failed", state.live == 0);

    /* Each allocation failure must leave no leaked partial registration. */
    for (size_t fail_call = 1; fail_call <= 4; fail_call++) {
        state = (gwatch__TestAllocator) {.fail_call = fail_call};
        watcher = gwatch_create(alloc);
        if (watcher) {
            gwatch_add(alloc, watcher, "glint-watcher-test.tmp");
            gwatch_destroy(alloc, watcher);
        }
        mewassert("Test expectation failed", state.live == 0);
    }
})

mewtest_main();

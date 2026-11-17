#include <glint/platform/watcher.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gwatch__TestAllocator {
    size_t live;
    size_t calls;
    size_t fail_call;
    bool denied;
} gwatch__TestAllocator;

static void* gwatch__test_alloc(void* data, size_t bytes);
static void gwatch__test_free(void* data, void* ptr);
static void* gwatch__test_calloc(void* data, size_t count, size_t bytes);
static void* gwatch__test_realloc(void* data, void* ptr, size_t old_size, size_t new_size);
static void gwatch__test_on_change(const char* path, void* user);

static const galloc_FunctionTable gwatch__test_table =
    {gwatch__test_alloc, gwatch__test_free, gwatch__test_calloc, gwatch__test_realloc};

typedef struct gwatch__TestContext {
    gwatch__TestAllocator state;
    galloc_Allocator alloc;
    gwatch_Watcher* watcher;
    FILE* file;
    const char* path;
    size_t changes;
} gwatch__TestContext;

static void gwatch__test_context_allocator(gwatch__TestContext* context);
static bool gwatch__test_context_create(gwatch__TestContext* context);
static void gwatch__test_context_free(void* user);
static bool gwatch__test_write_file(gwatch__TestContext* context, const char* contents);
static bool gwatch__test_failed_registration(gwatch__TestContext* context, size_t fail_call);

GUNIT_TEST(duplicate_registration_is_ignored,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gunit_assert("duplicate registration succeeds", gwatch_add(context.alloc, context.watcher, context.path));
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "created"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("duplicate registration emits only one change", context.changes == 1);
)

GUNIT_TEST(unchanged_missing_file_emits_nothing,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("missing file emits nothing", context.changes == 0);
)

GUNIT_TEST(file_creation_emits_change,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "first"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("file creation emits one change", context.changes == 1);
)

GUNIT_TEST(unchanged_existing_file_emits_nothing,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "first"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    context.changes = 0;
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("unchanged existing file emits nothing", context.changes == 0);
)

GUNIT_TEST(file_rewrite_emits_change,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "first"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    context.changes = 0;
    // A different size is visible even with coarse filesystem timestamps.
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "longer contents"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("rewrite emits one change", context.changes == 1);
)

GUNIT_TEST(file_deletion_emits_change,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "first"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    context.changes = 0;
    gunit_assert("remove watched fixture", remove(context.path) == 0);
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("deletion emits one change", context.changes == 1);
)

GUNIT_TEST(file_reappearance_emits_change,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "first"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("remove watched fixture", remove(context.path) == 0);
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    context.changes = 0;
    gunit_assert("write watched fixture", gwatch__test_write_file(&context, "restored"));
    gwatch_poll(context.watcher, gwatch__test_on_change, &context);
    gunit_assert("reappearance emits one change", context.changes == 1);
)

GUNIT_TEST(watcher_destroy_releases_allocations,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    gwatch__test_context_free(&context);
    gunit_assert("destroy releases all watcher allocations", context.state.live == 0);
)

GUNIT_TEST(watcher_allocation_failures_allow_retry,
    gwatch__TestContext context = {0};
    gunit_defer(gwatch__test_context_free, &context);
    gunit_assert("create watcher fixture", gwatch__test_context_create(&context));
    size_t allocation_count = context.state.calls;
    gunit_assert("watcher setup exercised allocator", allocation_count > 0);
    gwatch__test_context_free(&context);
    gunit_assert("baseline setup releases allocations", context.state.live == 0);
    for (size_t fail_call = 1; fail_call <= allocation_count; fail_call++) {
        context = (gwatch__TestContext) {0};
        gunit_assert("check allocation denial and retry", gwatch__test_failed_registration(&context, fail_call));
    }
)

gunit_main();

static void* gwatch__test_alloc(void* data, size_t bytes) {
    gwatch__TestAllocator* state = data;
    state->calls += 1;
    if (state->calls == state->fail_call) {
        state->denied = true;
        return NULL;
    }

    void* ptr = malloc(bytes);
    if (ptr != NULL) {
        state->live += 1;
    }

    return ptr;
}

static void gwatch__test_free(void* data, void* ptr) {
    gwatch__TestAllocator* state = data;
    if (ptr != NULL) {
        if (!gunit_check("free has a live allocation", state->live > 0)) {
            return;
        }
        state->live -= 1;
        free(ptr);
    }
}

static void* gwatch__test_calloc(void* data, size_t count, size_t bytes) {
    gwatch__TestAllocator* state = data;
    state->calls += 1;
    if (state->calls == state->fail_call) {
        state->denied = true;
        return NULL;
    }

    void* ptr = calloc(count, bytes);
    if (ptr != NULL) {
        state->live += 1;
    }

    return ptr;
}

static void* gwatch__test_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gwatch__TestAllocator* state = data;
    (void)old_size;
    state->calls += 1;
    if (state->calls == state->fail_call) {
        state->denied = true;
        return NULL;
    }

    bool is_new = ptr == NULL;
    void* result = realloc(ptr, new_size);
    if (result != NULL && is_new) {
        state->live += 1;
    }

    return result;
}

static void gwatch__test_on_change(const char* path, void* user) {
    gwatch__TestContext* context = user;
    if (!gunit_check("callback receives watched path", strcmp(path, context->path) == 0)) {
        return;
    }
    context->changes += 1;
}

static bool gwatch__test_write_file(gwatch__TestContext* context, const char* contents) {
    context->file = fopen(context->path, "wb");
    if (!gunit_check("open watched fixture", context->file != NULL)) {
        return false;
    }
    if (!gunit_check(
            "write watched fixture",
            fwrite(contents, 1, strlen(contents), context->file) == strlen(contents)
        )) {
        return false;
    }
    int closed = fclose(context->file);
    context->file = NULL;
    if (!gunit_check("close watched fixture", closed == 0)) {
        return false;
    }
    return true;
}

static void gwatch__test_context_allocator(gwatch__TestContext* context) {
    context->path = "glint-watcher-test.tmp";
    context->alloc = (galloc_Allocator) {.data = &context->state, .ftable = &gwatch__test_table};
    remove(context->path);
}

static bool gwatch__test_context_create(gwatch__TestContext* context) {
    gwatch__test_context_allocator(context);
    context->watcher = gwatch_create(context->alloc);
    if (!gunit_check("watcher created", context->watcher != NULL)) {
        return false;
    }
    if (!gunit_check("missing file registered", gwatch_add(context->alloc, context->watcher, context->path))) {
        return false;
    }
    return true;
}

static void gwatch__test_context_free(void* user) {
    gwatch__TestContext* context = user;
    if (context->file != NULL) {
        fclose(context->file);
        context->file = NULL;
    }
    if (context->watcher != NULL) {
        gwatch_destroy(context->alloc, context->watcher);
        context->watcher = NULL;
    }
    if (context->path != NULL) {
        remove(context->path);
    }
}

static bool gwatch__test_failed_registration(gwatch__TestContext* context, size_t fail_call) {
    gwatch__test_context_allocator(context);
    context->state.fail_call = fail_call;
    context->watcher = gwatch_create(context->alloc);
    bool registered = false;
    if (context->watcher != NULL) {
        registered = gwatch_add(context->alloc, context->watcher, context->path);
    }
    if (!gunit_check("injected allocation denial reached", context->state.denied)) {
        return false;
    }
    context->state.fail_call = 0;
    if (context->watcher == NULL) {
        context->watcher = gwatch_create(context->alloc);
        if (!gunit_check("creation can retry after allocation denial", context->watcher != NULL)) {
            return false;
        }
    }
    if (!registered) {
        if (!gwatch__test_write_file(context, "before retry")) {
            return false;
        }
        gwatch_poll(context->watcher, gwatch__test_on_change, context);
        if (!gunit_check("failed registration emits no change", context->changes == 0)) {
            return false;
        }
        if (!gunit_check("remove fixture before registration retry", remove(context->path) == 0)) {
            return false;
        }
    }
    // A watcher may recover from a denied optional allocation. Either outcome
    // must leave registration usable, without duplicate notifications.
    if (!gunit_check(
            "registration succeeds after allocation denial",
            gwatch_add(context->alloc, context->watcher, context->path)
        )) {
        return false;
    }
    if (!gwatch__test_write_file(context, "after retry")) {
        return false;
    }
    gwatch_poll(context->watcher, gwatch__test_on_change, context);
    if (!gunit_check("registration reports one subsequent change", context->changes == 1)) {
        return false;
    }
    gwatch__test_context_free(context);
    if (!gunit_check("allocation denial and retry release all allocations", context->state.live == 0)) {
        return false;
    }
    return true;
}

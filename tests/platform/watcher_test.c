#include <glint/platform/watcher.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gwatch_test__Allocator {
    size_t live;
    size_t calls;
    size_t fail_call;
    bool denied;
} gwatch_test__Allocator;

static void* gwatch_test__alloc(void* data, size_t bytes);
static void gwatch_test__free(void* data, void* ptr);
static void* gwatch_test__calloc(void* data, size_t count, size_t bytes);
static void* gwatch_test__realloc(void* data, void* ptr, size_t old_size, size_t new_size);
static void gwatch_test__on_change(const char* path, void* user);

static const galloc_FunctionTable gwatch_test__table = {
    .alloc = gwatch_test__alloc,
    .free = gwatch_test__free,
    .calloc = gwatch_test__calloc,
    .realloc = gwatch_test__realloc
};

typedef struct gwatch_test__Context {
    gwatch_test__Allocator state;
    galloc_Allocator alloc;
    gwatch_Watcher* watcher;
    FILE* file;
    const char* path;
    size_t changes;
} gwatch_test__Context;

static void gwatch_test__context_allocator(gwatch_test__Context* context);
static bool gwatch_test__context_create(gwatch_test__Context* context);
static void gwatch_test__context_free(void* user);
static bool gwatch_test__write_file(gwatch_test__Context* context, const char* contents);
static bool gwatch_test__failed_registration(gwatch_test__Context* context, size_t fail_call);

GUNIT_TEST(watcher_missing_file,
    gwatch_test__Context context = {0};

    GUNIT_SETUP({
        gunit_defer(gwatch_test__context_free, &context);
        gunit_assert("watch a missing file", gwatch_test__context_create(&context));
    })

    GUNIT_SECTION("polling an unchanged missing file emits nothing", {
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("missing file emits no changes", context.changes == 0);
    })

    GUNIT_SECTION("creating the file emits one change", {
        gunit_assert("create watched file", gwatch_test__write_file(&context, "first"));
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("file creation emits one change", context.changes == 1);
    })

    GUNIT_SECTION("registering the same file twice still emits one change", {
        gunit_assert("duplicate registration succeeds", gwatch_add(context.alloc, context.watcher, context.path));

        gunit_assert("create watched file", gwatch_test__write_file(&context, "created"));
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("duplicate registration emits only one change", context.changes == 1);
    })
)

GUNIT_TEST(watcher_existing_file,
    gwatch_test__Context context = {0};

    GUNIT_SETUP({
        gunit_defer(gwatch_test__context_free, &context);
        gunit_assert("watch a missing file", gwatch_test__context_create(&context));
        gunit_assert("create watched file", gwatch_test__write_file(&context, "first"));
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        context.changes = 0;
    })

    GUNIT_SECTION("polling an unchanged file emits nothing", {
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("unchanged file emits no changes", context.changes == 0);
    })

    GUNIT_SECTION("rewriting the file emits one change", {
        // A different size is visible even with coarse filesystem timestamps.
        gunit_assert("rewrite watched file", gwatch_test__write_file(&context, "longer contents"));
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("rewrite emits one change", context.changes == 1);
    })

    GUNIT_SECTION("deleting the file emits one change", {
        gunit_assert("remove watched file", remove(context.path) == 0);
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("deletion emits one change", context.changes == 1);
    })

    GUNIT_SECTION("restoring a deleted file emits one change", {
        gunit_assert("remove watched file", remove(context.path) == 0);
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        context.changes = 0;

        gunit_assert("restore watched file", gwatch_test__write_file(&context, "restored"));
        gwatch_poll(context.watcher, gwatch_test__on_change, &context);
        gunit_assert("reappearance emits one change", context.changes == 1);
    })
)

GUNIT_TEST(watcher_destroy_releases_allocations,
    gwatch_test__Context context = {0};
    gunit_defer(gwatch_test__context_free, &context);
    gunit_assert("create watcher fixture", gwatch_test__context_create(&context));
    gwatch_test__context_free(&context);

    gunit_assert("destroy releases all watcher allocations", context.state.live == 0);
)

GUNIT_TEST(watcher_allocation_failures_allow_retry,
    gwatch_test__Context context = {0};
    gunit_defer(gwatch_test__context_free, &context);
    gunit_assert("create watcher fixture", gwatch_test__context_create(&context));
    const size_t allocation_count = context.state.calls;
    gunit_assert("watcher setup exercised allocator", allocation_count > 0);
    gwatch_test__context_free(&context);
    gunit_assert("baseline setup releases allocations", context.state.live == 0);

    for (size_t fail_call = 1; fail_call <= allocation_count; fail_call++) {
        context = (gwatch_test__Context) {0};
        gunit_assert("check allocation denial and retry", gwatch_test__failed_registration(&context, fail_call));
    }
)

gunit_main();

static void* gwatch_test__alloc(void* data, size_t bytes) {
    gwatch_test__Allocator* state = data;
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

static void gwatch_test__free(void* data, void* ptr) {
    gwatch_test__Allocator* state = data;
    if (ptr != NULL) {
        if (!gunit_check("free has a live allocation", state->live > 0)) {
            return;
        }
        state->live -= 1;
        free(ptr);
    }
}

static void* gwatch_test__calloc(void* data, size_t count, size_t bytes) {
    gwatch_test__Allocator* state = data;
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

static void* gwatch_test__realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gwatch_test__Allocator* state = data;
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

static void gwatch_test__on_change(const char* path, void* user) {
    gwatch_test__Context* context = user;
    if (!gunit_check("callback receives watched path", strcmp(path, context->path) == 0)) {
        return;
    }
    context->changes += 1;
}

static bool gwatch_test__write_file(gwatch_test__Context* context, const char* contents) {
    context->file = fopen(context->path, "wb");
    if (!gunit_check("open watched fixture", context->file != NULL)) {
        return false;
    }
    const size_t length = strlen(contents);
    if (!gunit_check("write watched fixture", fwrite(contents, 1, length, context->file) == length)) {
        return false;
    }
    const int closed = fclose(context->file);
    context->file = NULL;
    return gunit_check("close watched fixture", closed == 0);
}

static void gwatch_test__context_allocator(gwatch_test__Context* context) {
    context->path = "glint-watcher-test.tmp";
    context->alloc = (galloc_Allocator) {.data = &context->state, .ftable = &gwatch_test__table};
    remove(context->path);
}

static bool gwatch_test__context_create(gwatch_test__Context* context) {
    gwatch_test__context_allocator(context);
    context->watcher = gwatch_create(context->alloc);
    if (!gunit_check("watcher created", context->watcher != NULL)) {
        return false;
    }
    return gunit_check("missing file registered", gwatch_add(context->alloc, context->watcher, context->path));
}

static void gwatch_test__context_free(void* user) {
    gwatch_test__Context* context = user;
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

static bool gwatch_test__failed_registration(gwatch_test__Context* context, size_t fail_call) {
    gwatch_test__context_allocator(context);
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
        if (!gwatch_test__write_file(context, "before retry")) {
            return false;
        }
        gwatch_poll(context->watcher, gwatch_test__on_change, context);
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
    if (!gwatch_test__write_file(context, "after retry")) {
        return false;
    }
    gwatch_poll(context->watcher, gwatch_test__on_change, context);
    if (!gunit_check("registration reports one subsequent change", context->changes == 1)) {
        return false;
    }
    gwatch_test__context_free(context);
    return gunit_check("allocation denial and retry release all allocations", context->state.live == 0);
}

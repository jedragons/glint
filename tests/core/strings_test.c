#include <glint/core/strings.h>
#include <glint/unit/unit.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct gstr__TestAllocator {
    galloc_Allocator backing;
    size_t live;
    bool fail;
} gstr__TestAllocator;

static void* gstr__test_alloc(void* data, size_t bytes);
static void gstr__test_free(void* data, void* ptr);
static void* gstr__test_calloc(void* data, size_t count, size_t size);
static void* gstr__test_realloc(void* data, void* ptr, size_t old_size, size_t new_size);
static bool gstr__test_append_twice(galloc_Allocator alloc, gstr_Buffer* sb, const char* format, ...);

typedef struct gstr__TestContext {
    gstr__TestAllocator state;
    galloc_Allocator alloc;
    gstr_Buffer buffer;
} gstr__TestContext;

static void gstr__test_context_create(gstr__TestContext* context);
static void gstr__test_context_free(void* user);

GUNIT_TEST(slice_trim_space, {
    gstr_Slice source = gstr_Slice_from_cstr(" \t hello\r\n");
    gunit_assert("trim both ends", gstr_Slice_eq(gstr_Slice_trim_space(source), gstr_Slice_from_cstr("hello")));
    gunit_return();
})

GUNIT_TEST(slice_trim_left, {
    gstr_Slice source = gstr_Slice_from_cstr(" \t hello\r\n");
    gunit_assert(
        "left trim preserves trailing whitespace",
        gstr_Slice_eq(gstr_Slice_trim_left(source), gstr_Slice_from_cstr("hello\r\n"))
    );
    gunit_return();
})

GUNIT_TEST(slice_trim_right, {
    gstr_Slice source = gstr_Slice_from_cstr(" \t hello\r\n");
    gunit_assert(
        "right trim preserves leading whitespace",
        gstr_Slice_eq(gstr_Slice_trim_right(source), gstr_Slice_from_cstr(" \t hello"))
    );
    gunit_return();
})

GUNIT_TEST(slice_trim_whitespace_only, {
    gstr_Slice source = gstr_Slice_from_cstr(" \t\n");
    gunit_assert("left trim removes whitespace-only slice", gstr_Slice_trim_left(source).count == 0);
    gunit_assert("right trim removes whitespace-only slice", gstr_Slice_trim_right(source).count == 0);
    gunit_return();
})

GUNIT_TEST(slice_trim_empty, {
    gunit_assert("left trim handles empty slice", gstr_Slice_trim_left((gstr_Slice) {0}).items == NULL);
    gunit_assert("right trim handles empty slice", gstr_Slice_trim_right((gstr_Slice) {0}).items == NULL);
    gunit_return();
})

GUNIT_TEST(slice_extracts_range, {
    gstr_Slice source = gstr_Slice_from_cstr(" \t hello\r\n");
    gunit_assert(
        "slice extracts requested range",
        gstr_Slice_eq(gstr_Slice_slice(source, 3, 5), gstr_Slice_from_cstr("hello"))
    );
    gunit_return();
})

GUNIT_TEST(slice_extracts_boundary_ranges, {
    gstr_Slice source = gstr_Slice_from_cstr("hello");
    gunit_assert("whole slice is preserved", gstr_Slice_eq(gstr_Slice_slice(source, 0, source.count), source));
    gunit_assert(
        "last byte can be sliced",
        gstr_Slice_eq(gstr_Slice_slice(source, source.count - 1, 1), gstr_Slice_from_cstr("o"))
    );
    gunit_assert("empty suffix has zero count", gstr_Slice_slice(source, source.count, 0).count == 0);
    gunit_return();
})

GUNIT_TEST(slice_compares_binary_data, {
    const char raw[] = {'a', '\0', 'b'};
    gunit_assert(
        "slices support embedded NUL",
        gstr_Slice_eq((gstr_Slice) {raw, sizeof(raw)}, (gstr_Slice) {raw, sizeof(raw)})
    );
    const char different[] = {'a', '\0', 'c'};
    gunit_assert(
        "binary comparison checks bytes after NUL",
        !gstr_Slice_eq((gstr_Slice) {raw, sizeof(raw)}, (gstr_Slice) {different, sizeof(different)})
    );
    gunit_return();
})

GUNIT_TEST(zero_buffer_is_empty, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("zero buffer is empty string", strcmp(gstr_Buffer_cstr(&context.buffer), "") == 0);
    gunit_assert("zero buffer has empty slice", gstr_Buffer_slice(&context.buffer).count == 0);
    gunit_assert("zero reserve succeeds", gstr_Buffer_reserve(context.alloc, &context.buffer, 0));
    gstr_Buffer_clear(&context.buffer);
    gunit_return();
})

GUNIT_TEST(buffer_accepts_empty_slice, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert(
        "append empty slice",
        gstr_Buffer_append_slice(context.alloc, &context.buffer, (gstr_Slice) {NULL, 0})
    );
    gunit_assert("buffer remains empty", gstr_Buffer_slice(&context.buffer).count == 0);
    gunit_return();
})

GUNIT_TEST(buffer_self_append, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("append string", gstr_Buffer_append_cstr(context.alloc, &context.buffer, "0123456789"));
    gstr_Slice alias = gstr_Buffer_slice(&context.buffer);
    gunit_assert("append aliased slice", gstr_Buffer_append_slice(context.alloc, &context.buffer, alias));
    gunit_assert("self append preserves bytes", strcmp(gstr_Buffer_cstr(&context.buffer), "01234567890123456789") == 0);
    gunit_return();
})

GUNIT_TEST(buffer_appends_binary_data, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    const char raw[] = {'a', '\0', 'b'};
    gunit_assert(
        "append embedded NUL",
        gstr_Buffer_append_slice(context.alloc, &context.buffer, (gstr_Slice) {raw, sizeof(raw)})
    );
    gunit_assert(
        "binary append retains terminator",
        context.buffer.count == sizeof(raw) && memcmp(context.buffer.items, raw, sizeof(raw)) == 0
            && context.buffer.items[sizeof(raw)] == '\0'
    );
    gunit_return();
})

GUNIT_TEST(buffer_copies_independent_slice, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    char source[] = "test";
    gunit_assert(
        "append independent slice",
        gstr_Buffer_append_slice(context.alloc, &context.buffer, (gstr_Slice) {source, 4})
    );
    source[0] = 'x';
    gunit_assert("buffer owns copied bytes", strcmp(gstr_Buffer_cstr(&context.buffer), "test") == 0);
    gunit_return();
})

GUNIT_TEST(buffer_reserve_failure_preserves_content, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("append original content", gstr_Buffer_append_cstr(context.alloc, &context.buffer, "original"));
    gunit_assert("fixture leaves room to request growth", context.buffer.capacity < SIZE_MAX);
    size_t required_capacity = context.buffer.capacity + 1;
    context.state.fail = true;
    gunit_assert(
        "required capacity growth reports failure",
        !gstr_Buffer_reserve(context.alloc, &context.buffer, required_capacity)
    );
    gunit_assert(
        "failed reserve preserves content",
        context.buffer.count == 8 && strcmp(gstr_Buffer_cstr(&context.buffer), "original") == 0
    );
    gunit_return();
})

GUNIT_TEST(buffer_appends_formatted_text, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("append formatted text", gstr_Buffer_appendf(context.alloc, &context.buffer, " %s %d", "value", 42));
    gunit_assert("formatted bytes match", strcmp(gstr_Buffer_cstr(&context.buffer), " value 42") == 0);
    gunit_return();
})

GUNIT_TEST(buffer_preserves_va_list, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("va_list can be reused", gstr__test_append_twice(context.alloc, &context.buffer, "%s %d", "x", 42));
    gunit_assert("formatted text appended twice", strcmp(gstr_Buffer_cstr(&context.buffer), "x 42x 42") == 0);
    gunit_return();
})

GUNIT_TEST(buffer_clear_leaves_empty_string, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("append initial string", gstr_Buffer_append_cstr(context.alloc, &context.buffer, "content"));
    gstr_Buffer_clear(&context.buffer);
    gunit_assert(
        "clear leaves empty string",
        context.buffer.count == 0 && strcmp(gstr_Buffer_cstr(&context.buffer), "") == 0
    );
    gunit_return();
})

GUNIT_TEST(buffer_appends_character, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("append one character", gstr_Buffer_append_char(context.alloc, &context.buffer, 'x'));
    gunit_assert("character append terminates string", strcmp(gstr_Buffer_cstr(&context.buffer), "x") == 0);
    gunit_return();
})

GUNIT_TEST(buffer_destroy_releases_storage, {
    gstr__TestContext context = {0};
    gunit_defer(gstr__test_context_free, &context);
    gstr__test_context_create(&context);
    gunit_assert("append initial string", gstr_Buffer_append_cstr(context.alloc, &context.buffer, "content"));
    gstr__test_context_free(&context);
    gunit_assert("buffer storage released", context.state.live == 0);
    gunit_return();
})

gunit_main();

static void* gstr__test_alloc(void* data, size_t bytes) {
    gstr__TestAllocator* state = data;
    if (state->fail) {
        return NULL;
    }

    void* result = galloc_alloc(state->backing, bytes);
    if (result != NULL) {
        state->live += 1;
    }

    return result;
}

static void gstr__test_free(void* data, void* ptr) {
    gstr__TestAllocator* state = data;
    if (!gunit_check("free has a live allocation", (state->live != 0))) {
        return;
    }
    state->live -= 1;
    galloc_free(state->backing, ptr);
}

static void* gstr__test_calloc(void* data, size_t count, size_t size) {
    gstr__TestAllocator* state = data;
    if (state->fail) {
        return NULL;
    }

    void* result = galloc_calloc(state->backing, count, size);
    if (result != NULL) {
        state->live += 1;
    }

    return result;
}

static void* gstr__test_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gstr__TestAllocator* state = data;
    if (state->fail) {
        return NULL;
    }

    void* result = galloc_realloc(state->backing, ptr, old_size, new_size);
    if (result != NULL && ptr == NULL) {
        state->live += 1;
    }

    return result;
}

static bool gstr__test_append_twice(galloc_Allocator alloc, gstr_Buffer* sb, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool ok = gstr_Buffer_vappendf(alloc, sb, format, args) && gstr_Buffer_vappendf(alloc, sb, format, args);
    va_end(args);
    return ok;
}

static void gstr__test_context_create(gstr__TestContext* context) {
    static const galloc_FunctionTable table =
        {gstr__test_alloc, gstr__test_free, gstr__test_calloc, gstr__test_realloc};
    context->state.backing = galloc_malloc_allocator();
    context->alloc = (galloc_Allocator) {.data = &context->state, .ftable = &table};
}

static void gstr__test_context_free(void* user) {
    gstr__TestContext* context = user;
    context->state.fail = false;
    gstr_Buffer_destroy(context->alloc, &context->buffer);
}

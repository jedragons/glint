#include <glint/strings.h>

#include <mewunit.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct gstr__TestAllocator {
    galloc_Allocator backing;
    size_t live;
    bool fail;
} gstr__TestAllocator;

static void *gstr__test_test_alloc(void *data, size_t bytes) {
    gstr__TestAllocator *state = data;
    if (state->fail)
        return NULL;
    void *result = galloc_alloc(state->backing, bytes);
    if (result)
        state->live++;
    return result;
}

static void gstr__test_test_free(void *data, void *ptr) {
    gstr__TestAllocator *state = data;
    mewrequire("Test expectation failed", state->live);
    state->live--;
    galloc_free(state->backing, ptr);
}

static void *gstr__test_test_calloc(void *data, size_t count, size_t size) {
    gstr__TestAllocator *state = data;
    if (state->fail)
        return NULL;
    void *result = galloc_calloc(state->backing, count, size);
    if (result)
        state->live++;
    return result;
}

static void *gstr__test_test_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gstr__TestAllocator *state = data;
    if (state->fail)
        return NULL;
    void *result = galloc_realloc(state->backing, ptr, old_size, new_size);
    if (result && !ptr)
        state->live++;
    return result;
}

TEST(strings_test, {
    static const galloc_FunctionTable gstr__test_table = {gstr__test_test_alloc, gstr__test_test_free, gstr__test_test_calloc, gstr__test_test_realloc};
    gstr__TestAllocator state = {.backing = galloc_malloc_allocator()};
    galloc_Allocator alloc = {.data = &state, .ftable = &gstr__test_table};
    gstr_View source = gstr_View_from_cstr(" \t hello\r\n");
    mewassert("Test expectation failed", gstr_View_eq(gstr_View_trim_space(source), gstr_View_from_cstr("hello")));
    mewassert("Test expectation failed", gstr_View_eq(gstr_View_slice(source, 3, 5), gstr_View_from_cstr("hello")));
    mewassert("Test expectation failed", !gstr_View_slice(source, SIZE_MAX, 2).items);
    mewassert("Test expectation failed", !gstr_View_slice(source, 1, SIZE_MAX).items);
    const char raw[] = {'a', '\0', 'b'};
    mewassert("Test expectation failed", gstr_View_eq((gstr_View) {raw, sizeof(raw)}, (gstr_View) {raw, sizeof(raw)}));
    mewassert("Test expectation failed", !gstr_View_eq((gstr_View) {NULL, 1}, (gstr_View) {raw, 1}));

    gstr_Builder sb;
    gstr_Builder_init(&sb);
    mewassert("Test expectation failed", !strcmp(gstr_Builder_cstr(&sb), ""));
    mewassert("Test expectation failed", gstr_Builder_append_sv(alloc, &sb, (gstr_View) {NULL, 0}));
    mewassert("Test expectation failed", gstr_Builder_append_cstr(alloc, &sb, "0123456789"));
    gstr_View alias = gstr_Builder_view(&sb);
    mewassert("Test expectation failed", gstr_Builder_append_sv(alloc, &sb, alias));
    mewassert("Test expectation failed", !strcmp(gstr_Builder_cstr(&sb), "01234567890123456789"));
    mewassert("Test expectation failed", gstr_Builder_append_sv(alloc, &sb, (gstr_View) {raw, sizeof(raw)}));
    mewassert("Test expectation failed", sb.count == 23 && !memcmp(sb.items + 20, raw, sizeof(raw)) && sb.items[23] == '\0');
    mewassert("Test expectation failed", !gstr_Builder_append_sv(alloc, &sb, (gstr_View) {sb.items + sb.count, 1}));
    size_t before = sb.count;
    state.fail = true;
    mewassert("Test expectation failed", !gstr_Builder_reserve(alloc, &sb, 10000));
    mewassert("Test expectation failed", !gstr_Builder_append_sv(alloc, &sb, (gstr_View) {raw, SIZE_MAX}));
    mewassert("Test expectation failed", !gstr_Builder_appendf(alloc, &sb, "%d", 1));
    mewassert("Test expectation failed", sb.count == before && sb.items[before] == '\0');
    state.fail = false;
    mewassert("Test expectation failed", gstr_Builder_appendf(alloc, &sb, " %s %d", "value", 42));
    mewassert("Test expectation failed", !strcmp(sb.items + before, " value 42"));
    gstr_Builder_clear(&sb);
    mewassert("Test expectation failed", sb.count == 0 && !strcmp(gstr_Builder_cstr(&sb), ""));
    mewassert("Test expectation failed", gstr_Builder_append_char(alloc, &sb, 'x'));
    mewassert("Test expectation failed", !strcmp(gstr_Builder_cstr(&sb), "x"));
    gstr_Builder_destroy(alloc, &sb);
    mewassert("Test expectation failed", state.live == 0);
})

mewtest_main();

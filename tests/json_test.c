#include <glint/json.h>
#include <mewunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t attempts;
    size_t fail_at;
    size_t live;
} gjson__TestTrackingAllocator;

static void *gjson__test_tracking_alloc(void *data, size_t size) {
    gjson__TestTrackingAllocator *tracking = data;
    void *ptr;
    if (++tracking->attempts == tracking->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        ++tracking->live;
    return ptr;
}

static void gjson__test_tracking_free(void *data, void *ptr) {
    gjson__TestTrackingAllocator *tracking = data;
    if (ptr) {
        mewrequire("Test expectation failed", tracking->live);
        --tracking->live;
        free(ptr);
    }
}

static void *gjson__test_tracking_calloc(void *data, size_t count, size_t size) {
    void *ptr = gjson__test_tracking_alloc(data, count * size);
    if (ptr)
        memset(ptr, 0, count * size);
    return ptr;
}

static void *gjson__test_tracking_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gjson__TestTrackingAllocator *tracking = data;
    (void)old_size;
    if (!ptr)
        return gjson__test_tracking_alloc(data, new_size);
    if (++tracking->attempts == tracking->fail_at)
        return NULL;
    return realloc(ptr, new_size);
}

static void gjson__test_test_allocator(const char *input) {
    static const galloc_FunctionTable gjson__test_table = {gjson__test_tracking_alloc, gjson__test_tracking_free, gjson__test_tracking_calloc, gjson__test_tracking_realloc};
    gjson__TestTrackingAllocator tracking = {0};
    galloc_Allocator alloc = {&tracking, &gjson__test_table};
    gjson_Value *v = gjson_parse(alloc, input, strlen(input), NULL);
    size_t attempts = tracking.attempts;
    size_t i;
    mewassert("Test expectation failed", v && attempts);
    gjson_Value_free(alloc, v);
    mewassert("Test expectation failed", tracking.live == 0);
    for (i = 1; i <= attempts; ++i) {
        tracking.attempts = 0;
        tracking.fail_at = i;
        v = gjson_parse(alloc, input, strlen(input), NULL);
        mewassert("Test expectation failed", !v);
        mewassert("Test expectation failed", tracking.live == 0);
    }
}

static gjson_Value *gjson__test_parse(const char *s) {
    return gjson_parse(galloc_malloc_allocator(), s, strlen(s), NULL);
}

static void gjson__test_invalid(const char *s) {
    gjson_Value *v = gjson__test_parse(s);
    mewassert("Test expectation failed", !v);
}

TEST(json_test, {
    const char *doc =
        "{\"frames\":{\"frame0\":{\"x\":0,\"w\":16,\"duration\":150}},\"animations\":{\"walk\":[\"frame0\",\"frame1\"]},\"str\":\"\\uD83D\\uDE03\\u0000a\\n\",\"flag\":true,\"none\":null,\"n\":-1.25e+2}";
    gjson_Value *v = gjson__test_parse(doc);
    gjson__test_test_allocator(doc);
    const gjson_Value *frames, *frame, *a, *s;
    mewassert("Test expectation failed", v && v->type == GJSON_OBJECT && v->as.children.length == 6);
    frames = gjson_Value_get(v, "frames");
    frame = gjson_Value_get(frames, "frame0");
    mewassert("Test expectation failed", gjson_Value_get(frame, "w")->as.number == 16);
    a = gjson_Value_get(gjson_Value_get(v, "animations"), "walk");
    mewassert("Test expectation failed", a && a->type == GJSON_ARRAY && a->as.children.length == 2);
    mewassert("Test expectation failed", strcmp(gjson_Value_at(a, 1)->as.string.data, "frame1") == 0);
    mewassert("Test expectation failed", !gjson_Value_at(a, 2));
    s = gjson_Value_get(v, "str");
    mewassert("Test expectation failed", s->as.string.length == 7 && memcmp(s->as.string.data, "\xf0\x9f\x98\x83\0a\n", 7) == 0);
    mewassert("Test expectation failed", gjson_Value_get(v, "n")->as.number == -125);
    mewassert("Test expectation failed", gjson_Value_get(v, "flag")->as.boolean == 1);
    mewassert("Test expectation failed", gjson_Value_get(v, "none")->type == GJSON_NULL);
    gjson_Value_free(galloc_malloc_allocator(), v);
    v = gjson__test_parse("{\"x\\u0000y\":1,\"\":false}");
    mewassert("Test expectation failed", v && gjson_Value_get_n(v, "x\0y", 3)->as.number == 1 && gjson_Value_get(v, ""));
    gjson_Value_free(galloc_malloc_allocator(), v);
    gjson__test_invalid("");
    gjson__test_invalid("[1,]");
    gjson__test_invalid("{\"x\":1,}");
    gjson__test_invalid("{x:1}");
    gjson__test_invalid("01");
    gjson__test_invalid("-.1");
    gjson__test_invalid("1.");
    gjson__test_invalid("1e+");
    gjson__test_invalid("true false");
    gjson__test_invalid("\"\\ud800\"");
    gjson__test_invalid("\"\\udc00\"");
    gjson__test_invalid("\"\\ud800\\u1234\"");
    gjson__test_invalid("\"\\x00\"");
    gjson__test_invalid("\"\001\"");
    gjson__test_invalid("\"\xc0\x80\"");
    puts("json tests passed");
})

mewtest_main();

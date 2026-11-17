#include <glint/json/json.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t attempts;
    size_t fail_at;
    size_t live;
} gjson__TestTrackingAllocator;

static void* gjson__test_tracking_alloc(void* data, size_t size);
static void gjson__test_tracking_free(void* data, void* ptr);
static void* gjson__test_tracking_calloc(void* data, size_t count, size_t size);
static void* gjson__test_tracking_realloc(void* data, void* ptr, size_t old_size, size_t new_size);

typedef struct gjson__TestContext {
    galloc_Allocator alloc;
    gjson__TestTrackingAllocator tracking;
    gjson_Value* value;
} gjson__TestContext;

static void gjson__test_create(gjson__TestContext* context);
static void gjson__test_free(void* user);
static bool gjson__test_parse(gjson__TestContext* context, const char* source);

static const char* gjson__test_document =
    "{\"frames\":{\"frame0\":{\"x\":0,\"w\":16,\"duration\":150}},"
    "\"animations\":{\"walk\":[\"frame0\",\"frame1\"]},"
    "\"str\":\"\\uD83D\\uDE03\\u0000a\\n\",\"flag\":true,\"none\":null,\"n\":-1.25e+2}";

GUNIT_TEST(json_object_members,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, gjson__test_document));
    gunit_assert(
        "object has expected members",
        context.value->type == GJSON_OBJECT && context.value->as.children.length == 6
    );
)

GUNIT_TEST(json_nested_number,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, gjson__test_document));
    const gjson_Value* frames = gjson_Value_get(context.value, "frames");
    const gjson_Value* frame = gjson_Value_get(frames, "frame0");
    gunit_assert("read nested number", gjson_Value_get(frame, "w")->as.number == 16);
)

GUNIT_TEST(json_array_indexing,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, gjson__test_document));
    const gjson_Value* array = gjson_Value_get(gjson_Value_get(context.value, "animations"), "walk");
    gunit_assert(
        "parse animation array",
        array != NULL && array->type == GJSON_ARRAY && array->as.children.length == 2
    );
    gunit_assert("read array string", strcmp(gjson_Value_at(array, 1)->as.string.data, "frame1") == 0);
    gunit_assert("out of range returns NULL", gjson_Value_at(array, 2) == NULL);
)

GUNIT_TEST(json_unicode_and_embedded_nul_string,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, gjson__test_document));
    const gjson_Value* value = gjson_Value_get(context.value, "str");
    gunit_assert(
        "Unicode and embedded NUL decoded",
        value->as.string.length == 7 && memcmp(value->as.string.data, "\xf0\x9f\x98\x83\0a\n", 7) == 0
    );
)

GUNIT_TEST(json_numeric_exponent,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, "-1.25e+2"));
    gunit_assert("numeric exponent parsed", context.value->type == GJSON_NUMBER && context.value->as.number == -125);
)

GUNIT_TEST(json_true_literal,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, "true"));
    gunit_assert("true literal parsed", context.value->type == GJSON_BOOL && context.value->as.boolean);
)

GUNIT_TEST(json_false_literal,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, "false"));
    gunit_assert("false literal parsed", context.value->type == GJSON_BOOL && !context.value->as.boolean);
)

GUNIT_TEST(json_null_literal,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, "null"));
    gunit_assert("null literal parsed", context.value->type == GJSON_NULL);
)

GUNIT_TEST(json_embedded_nul_object_key,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, "{\"x\\u0000y\":1,\"\":false}"));
    gunit_assert("length-aware key lookup", gjson_Value_get_n(context.value, "x\0y", 3)->as.number == 1);
)

GUNIT_TEST(json_empty_object_key,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, "{\"x\\u0000y\":1,\"\":false}"));
    const gjson_Value* value = gjson_Value_get(context.value, "");
    gunit_assert("empty key lookup", value != NULL && value->type == GJSON_BOOL && !value->as.boolean);
)

GUNIT_TEST(json_tree_free_releases_allocations,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, gjson__test_document));
    gunit_assert("parse allocates", context.tracking.attempts > 0);
    gjson_Value_free(context.alloc, context.value);
    context.value = NULL;
    gunit_assert("parsed tree releases memory", context.tracking.live == 0);
)

GUNIT_TEST(json_allocation_failures_release_partial_tree,
    gjson__TestContext context = {0};
    gunit_defer(gjson__test_free, &context);
    gjson__test_create(&context);
    gunit_assert("parse JSON fixture", gjson__test_parse(&context, gjson__test_document));
    size_t attempts = context.tracking.attempts;
    gunit_assert("tracked parse allocates", attempts > 0);
    gjson_Value_free(context.alloc, context.value);
    context.value = NULL;
    for (size_t failure = 1; failure <= attempts; failure++) {
        context.tracking.attempts = 0;
        context.tracking.fail_at = failure;
        context.value = gjson_parse(context.alloc, gjson__test_document, strlen(gjson__test_document), NULL);
        gunit_assert("allocation failure reported", context.value == NULL);
        gunit_assert("failed parse releases partial tree", context.tracking.live == 0);
    }
)

GUNIT_TEST(json_rejects_malformed_inputs,
    static const struct {
        const char* source;
        const char* message;
    } cases[] = {
        {"", "reject empty document"},
        {"[1,]", "reject array trailing comma"},
        {"{\"x\":1,}", "reject object trailing comma"},
        {"{x:1}", "reject unquoted key"},
        {"01", "reject leading zero"},
        {"-.1", "reject missing integer part"},
        {"1.", "reject missing fraction digits"},
        {"1e+", "reject missing exponent digits"},
        {"true false", "reject multiple roots"},
        {"tru", "reject incomplete true"},
        {"nul", "reject incomplete null"},
        {"\"\\ud800\"", "reject unpaired high surrogate"},
        {"\"\\udc00\"", "reject unpaired low surrogate"},
        {"\"\\ud800\\u1234\"", "reject invalid surrogate pair"},
        {"\"\\x00\"", "reject unsupported escape"},
        {"\"\001\"", "reject unescaped control character"},
        {"\"\xc0\x80\"", "reject overlong UTF-8"}
    };
    gjson__TestContext context = {0};
    gjson__test_create(&context);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char* source = cases[i].source;
        gjson_Value* value = gjson_parse(context.alloc, source, strlen(source), NULL);
        if (!gunit_check(cases[i].message, value == NULL)) {
            gjson_Value_free(context.alloc, value);
        }
    }
)

gunit_main();

static void gjson__test_create(gjson__TestContext* context) {
    static const galloc_FunctionTable functions = {
        .alloc = gjson__test_tracking_alloc,
        .free = gjson__test_tracking_free,
        .calloc = gjson__test_tracking_calloc,
        .realloc = gjson__test_tracking_realloc
    };
    context->alloc = (galloc_Allocator) {.data = &context->tracking, .ftable = &functions};
}

static void gjson__test_free(void* user) {
    gjson__TestContext* context = user;
    context->tracking.fail_at = 0;
    if (context->value != NULL) {
        gjson_Value_free(context->alloc, context->value);
    }
}

static bool gjson__test_parse(gjson__TestContext* context, const char* source) {
    context->value = gjson_parse(context->alloc, source, strlen(source), NULL);
    if (!gunit_check("parse JSON document", context->value != NULL)) {
        return false;
    }
    return true;
}

static void* gjson__test_tracking_alloc(void* data, size_t size) {
    gjson__TestTrackingAllocator* tracking = data;
    tracking->attempts += 1;
    if (tracking->attempts == tracking->fail_at) {
        return NULL;
    }

    void* ptr = malloc(size);
    if (ptr != NULL) {
        tracking->live += 1;
    }

    return ptr;
}

static void gjson__test_tracking_free(void* data, void* ptr) {
    gjson__TestTrackingAllocator* tracking = data;
    if (ptr != NULL) {
        if (!gunit_check("free has a live allocation", (tracking->live != 0))) {
            return;
        }
        tracking->live -= 1;
        free(ptr);
    }
}

static void* gjson__test_tracking_calloc(void* data, size_t count, size_t size) {
    void* ptr = gjson__test_tracking_alloc(data, count * size);
    if (ptr != NULL) {
        memset(ptr, 0, count * size);
    }

    return ptr;
}

static void* gjson__test_tracking_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gjson__TestTrackingAllocator* tracking = data;
    (void)old_size;
    if (ptr == NULL) {
        return gjson__test_tracking_alloc(data, new_size);
    }

    tracking->attempts += 1;
    if (tracking->attempts == tracking->fail_at) {
        return NULL;
    }

    return realloc(ptr, new_size);
}

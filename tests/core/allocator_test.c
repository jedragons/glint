#include <glint/config.h>
#include <glint/core/alloc.h>
#include <glint/core/arena.h>
#include <glint/ecs/ecs.h>
#include <glint/unit/unit.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if GLINT_COMPILER_MSVC
typedef union galloc__TestMaxAlign {
    long double floating;
    int64_t integer;
    void* pointer;
    void (*function)(void);
} galloc__TestMaxAlign;
#else
typedef max_align_t galloc__TestMaxAlign;
#endif

typedef struct galloc__TestCounts {
    size_t allocations;
    size_t frees;
    bool fail;
} galloc__TestCounts;

static void* galloc__test_count_alloc(void* data, size_t size);
static void galloc__test_count_free(void* data, void* ptr);
static void* galloc__test_count_calloc(void* data, size_t count, size_t size);
static void* galloc__test_count_realloc(void* data, void* ptr, size_t old_size, size_t new_size);

typedef struct galloc__TestContext {
    galloc__TestCounts counts;
    galloc_Allocator backing;
    garena_Arena arena;
    galloc_Allocator alloc;
    gecs_World* world;
    bool arena_initialized;
} galloc__TestContext;

static void galloc__test_context_create(galloc__TestContext* context);
static void galloc__test_context_free(void* user);

GUNIT_TEST(arena_alignment,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);

    void* first = galloc_alloc(context.alloc, 17);
    void* second = galloc_alloc(context.alloc, 80);
    gunit_assert("17-byte allocation succeeds", first != NULL);
    gunit_assert("80-byte allocation succeeds", second != NULL);
    gunit_assert("first block alignment", (uintptr_t)first % _Alignof(galloc__TestMaxAlign) == 0);
    gunit_assert("second block alignment", (uintptr_t)second % _Alignof(galloc__TestMaxAlign) == 0);
)

GUNIT_TEST(arena_reallocation_preserves_bytes,
    galloc__TestContext context = {0};
    char* original = NULL;

    GUNIT_SETUP({
        gunit_defer(galloc__test_context_free, &context);
        galloc__test_context_create(&context);

        original = galloc_alloc(context.alloc, 17);
        gunit_assert("initial allocation succeeds", original != NULL);
        memcpy(original, "arena", 6);
    })

    GUNIT_SECTION("growth preserves bytes", {
        char* larger = galloc_realloc(context.alloc, original, 17, 200);
        gunit_assert("grow the allocation", larger != NULL);
        gunit_assert("reallocation preserves bytes", memcmp(larger, "arena", 6) == 0);
    })

    GUNIT_SECTION("denied growth keeps the original bytes for a retry", {
        context.counts.fail = true;
        char* larger = galloc_realloc(context.alloc, original, 17, 10000);
        if (larger == NULL) {
            gunit_assert("failed reallocation preserves original bytes", memcmp(original, "arena", 6) == 0);

            context.counts.fail = false;
            larger = galloc_realloc(context.alloc, original, 17, 10000);
            gunit_assert("reallocation can retry after backing recovers", larger != NULL);
        }
        gunit_assert("successful reallocation preserves bytes", memcmp(larger, "arena", 6) == 0);
    })
)

GUNIT_TEST(arena_rejects_overflow,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);

    gunit_assert("overflowing array rejected", galloc_calloc(context.alloc, SIZE_MAX, 2) == NULL);
    gunit_assert("overflowing allocation rejected", galloc_alloc(context.alloc, SIZE_MAX) == NULL);
)

GUNIT_TEST(arena_reset_allows_reusable_allocations,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);

    const size_t sizes[] = {17, 80, 200};
    for (size_t pass = 0; pass < 3; pass++) {
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            unsigned char* bytes = galloc_alloc(context.alloc, sizes[i]);
            gunit_assert("allocation after reset succeeds", bytes != NULL);
            memset(bytes, (int)(pass + i), sizes[i]);
            gunit_assert("allocation remains writable", bytes[0] == pass + i && bytes[sizes[i] - 1] == pass + i);
        }
        garena_reset(&context.arena);
    }
    galloc__test_context_free(&context);
    gunit_assert("reusable arena releases all backing allocations", context.counts.frees == context.counts.allocations);
)

GUNIT_TEST(arena_calloc_clears_reused_memory,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);

    void* data = galloc_alloc(context.alloc, 16);
    gunit_assert("initial allocation succeeds", data != NULL);
    memset(data, 42, 16);
    garena_reset(&context.arena);

    char* zeroed = galloc_calloc(context.alloc, 4, 4);
    gunit_assert("calloc succeeds", zeroed != NULL);
    for (size_t i = 0; i < 16; i++) {
        gunit_assert("calloc clears every byte", zeroed[i] == 0);
    }
)

GUNIT_TEST(arena_destroy_releases_blocks,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);

    gunit_assert("first block allocated", galloc_alloc(context.alloc, 17) != NULL);
    gunit_assert("second block allocated", galloc_alloc(context.alloc, 80) != NULL);
    galloc__test_context_free(&context);
    gunit_assert("destroy frees all backing blocks", context.counts.frees == context.counts.allocations);
)

GUNIT_TEST(world_exposes_backing_allocator,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);
    context.world = gecs_World_new(context.backing);
    gunit_assert("world created with custom allocator", context.world != NULL);

    galloc_Allocator alloc = glint_allocator(context.world);
    gunit_assert(
        "world exposes backing allocator",
        alloc.data == context.backing.data && alloc.ftable == context.backing.ftable
    );
    gunit_assert("world allocates entities through its allocator", gecs_spawn(context.world).id != 0);
    galloc__test_context_free(&context);
    gunit_assert("world releases backing allocations", context.counts.frees == context.counts.allocations);
)

GUNIT_TEST(world_temporary_allocator_resets,
    galloc__TestContext context = {0};
    gunit_defer(galloc__test_context_free, &context);
    galloc__test_context_create(&context);
    context.world = gecs_World_new(context.backing);
    gunit_assert("world created", context.world != NULL);

    galloc_Allocator temp = glint_temp_allocator(context.world);
    unsigned char* first = galloc_alloc(temp, 64);
    gunit_assert("temporary allocation succeeds", first != NULL);
    memset(first, 17, 64);
    gecs_World_reset_temp(context.world);

    unsigned char* next = galloc_alloc(temp, 64);
    gunit_assert("temporary allocator works after reset", next != NULL);
    memset(next, 42, 64);
    gunit_assert("new temporary storage is writable", next[0] == 42 && next[63] == 42);
    galloc__test_context_free(&context);
    gunit_assert("world and arena release backing allocations", context.counts.frees == context.counts.allocations);
)

gunit_main();

static void* galloc__test_count_alloc(void* data, size_t size) {
    galloc__TestCounts* counts = data;
    if (counts->fail) {
        return NULL;
    }

    void* result = malloc(size);
    if (result != NULL) {
        counts->allocations += 1;
    }
    return result;
}

static void galloc__test_count_free(void* data, void* ptr) {
    galloc__TestCounts* counts = data;
    if (ptr != NULL) {
        counts->frees += 1;
        free(ptr);
    }
}

static void* galloc__test_count_calloc(void* data, size_t count, size_t size) {
    galloc__TestCounts* counts = data;
    if (counts->fail) {
        return NULL;
    }
    void* result = calloc(count, size);
    if (result != NULL) {
        counts->allocations += 1;
    }
    return result;
}

static void* galloc__test_count_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    galloc__TestCounts* counts = data;
    (void)old_size;
    if (counts->fail) {
        return NULL;
    }
    bool fresh = ptr == NULL;
    void* result = realloc(ptr, new_size);
    if (result != NULL && fresh) {
        counts->allocations += 1;
    }
    return result;
}

static void galloc__test_context_create(galloc__TestContext* context) {
    static const galloc_FunctionTable table = {
        galloc__test_count_alloc,
        galloc__test_count_free,
        galloc__test_count_calloc,
        galloc__test_count_realloc,
    };
    context->backing = (galloc_Allocator) {.data = &context->counts, .ftable = &table};
    garena_init(&context->arena, context->backing, 64);
    context->arena_initialized = true;
    context->alloc = garena_allocator(&context->arena);
}

static void galloc__test_context_free(void* user) {
    galloc__TestContext* context = user;
    if (context->world != NULL) {
        gecs_World_free(context->world);
        context->world = NULL;
    }
    if (context->arena_initialized) {
        garena_destroy(&context->arena);
        context->arena_initialized = false;
    }
}

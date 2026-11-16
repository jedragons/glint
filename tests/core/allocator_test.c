#include <glint/config.h>
#include <glint/core/alloc.h>
#include <glint/core/arena.h>
#include <glint/ecs/ecs.h>
#include <glint/unit/unit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if GLINT_COMPILER_MSVC
typedef union gecs__MaxAlign {
    long double floating;
    long long integer;
    void* pointer;
    void (*function)(void);
} gecs__MaxAlign;
#else
typedef max_align_t gecs__MaxAlign;
#endif

typedef struct gecs__TestCounts {
    size_t allocations;
    size_t frees;
    bool fail;
} gecs__TestCounts;

static void* gecs__test_count_alloc(void* data, size_t size);
static void gecs__test_count_free(void* data, void* ptr);
static void* gecs__test_count_calloc(void* data, size_t count, size_t size);
static void* gecs__test_count_realloc(void* data, void* ptr, size_t old_size, size_t new_size);

typedef struct gecs__AllocatorContext {
    gecs__TestCounts counts;
    galloc_Allocator backing;
    garena_Arena arena;
    galloc_Allocator alloc;
    gecs_World* world;
    bool arena_initialized;
} gecs__AllocatorContext;

static bool gecs__test_context_create(gecs__AllocatorContext* context);
static void gecs__test_context_free(void* user);

GUNIT_TEST(arena_alignment, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    void* first = galloc_alloc(context.alloc, 17);
    void* second = galloc_alloc(context.alloc, 80);
    gunit_assert("arena allocations succeed", first != NULL && second != NULL);
    gunit_assert("first block alignment", (uintptr_t)first % _Alignof(gecs__MaxAlign) == 0);
    gunit_assert("second block alignment", (uintptr_t)second % _Alignof(gecs__MaxAlign) == 0);
    gunit_return();
})

GUNIT_TEST(arena_reallocation_preserves_bytes, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    char* first = galloc_alloc(context.alloc, 17);
    gunit_assert("initial allocation succeeds", first != NULL);
    memcpy(first, "arena", 6);
    char* larger = galloc_realloc(context.alloc, first, 17, 200);
    gunit_assert("reallocation preserves bytes", larger != NULL && memcmp(larger, "arena", 6) == 0);
    gunit_return();
})

GUNIT_TEST(arena_reallocation_with_failing_backing_preserves_bytes, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    char* first = galloc_alloc(context.alloc, 17);
    gunit_assert("initial allocation succeeds", first != NULL);
    memcpy(first, "arena", 6);
    context.counts.fail = true;
    char* larger = galloc_realloc(context.alloc, first, 17, 10000);
    if (larger == NULL) {
        gunit_assert("failed reallocation preserves original bytes", memcmp(first, "arena", 6) == 0);
        context.counts.fail = false;
        larger = galloc_realloc(context.alloc, first, 17, 10000);
        gunit_assert("reallocation can retry after backing recovers", larger != NULL);
    }
    gunit_assert("successful reallocation preserves bytes", memcmp(larger, "arena", 6) == 0);
    gunit_return();
})

GUNIT_TEST(arena_rejects_overflow, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("overflowing array rejected", galloc_calloc(context.alloc, SIZE_MAX, 2) == NULL);
    gunit_assert("overflowing allocation rejected", galloc_alloc(context.alloc, SIZE_MAX) == NULL);
    gunit_return();
})

GUNIT_TEST(arena_reset_allows_reusable_allocations, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
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
    gecs__test_context_free(&context);
    gunit_assert("reusable arena releases all backing allocations", context.counts.frees == context.counts.allocations);
    gunit_return();
})

GUNIT_TEST(arena_calloc_clears_reused_memory, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    void* data = galloc_alloc(context.alloc, 16);
    gunit_assert("initial allocation succeeds", data != NULL);
    memset(data, 42, 16);
    garena_reset(&context.arena);
    char* zeroed = galloc_calloc(context.alloc, 4, 4);
    gunit_assert("calloc succeeds", zeroed != NULL);
    for (size_t i = 0; i < 16; i++) {
        gunit_assert("calloc clears every byte", zeroed[i] == 0);
    }
    gunit_return();
})

GUNIT_TEST(arena_destroy_releases_blocks, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("first block allocated", galloc_alloc(context.alloc, 17) != NULL);
    gunit_assert("second block allocated", galloc_alloc(context.alloc, 80) != NULL);
    gecs__test_context_free(&context);
    gunit_assert("destroy frees all backing blocks", context.counts.frees == context.counts.allocations);
    gunit_return();
})

GUNIT_TEST(world_exposes_backing_allocator, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    context.world = gecs_World_new(context.backing);
    gunit_assert("world created with custom allocator", context.world != NULL);
    galloc_Allocator alloc = glint_allocator(context.world);
    gunit_assert(
        "world exposes backing allocator",
        alloc.data == context.backing.data && alloc.ftable == context.backing.ftable
    );
    gunit_assert("world allocates entities through its allocator", gecs_spawn(context.world).id != 0);
    gecs__test_context_free(&context);
    gunit_assert("world releases backing allocations", context.counts.frees == context.counts.allocations);
    gunit_return();
})

GUNIT_TEST(world_temporary_allocator_resets, {
    gecs__AllocatorContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
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
    gecs__test_context_free(&context);
    gunit_assert("world and arena release backing allocations", context.counts.frees == context.counts.allocations);
    gunit_return();
})

gunit_main();

static void* gecs__test_count_alloc(void* data, size_t size) {
    gecs__TestCounts* counts = data;
    if (counts->fail) {
        return NULL;
    }

    void* result = malloc(size);
    if (result != NULL) {
        counts->allocations += 1;
    }
    return result;
}

static void gecs__test_count_free(void* data, void* ptr) {
    gecs__TestCounts* counts = data;
    if (ptr != NULL) {
        counts->frees += 1;
        free(ptr);
    }
}

static void* gecs__test_count_calloc(void* data, size_t count, size_t size) {
    gecs__TestCounts* counts = data;
    if (counts->fail) {
        return NULL;
    }
    void* result = calloc(count, size);
    if (result != NULL) {
        counts->allocations += 1;
    }
    return result;
}

static void* gecs__test_count_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gecs__TestCounts* counts = data;
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

static bool gecs__test_context_create(gecs__AllocatorContext* context) {
    static const galloc_FunctionTable table = {
        gecs__test_count_alloc,
        gecs__test_count_free,
        gecs__test_count_calloc,
        gecs__test_count_realloc,
    };
    context->backing = (galloc_Allocator) {.data = &context->counts, .ftable = &table};
    garena_init(&context->arena, context->backing, 64);
    context->arena_initialized = true;
    context->alloc = garena_allocator(&context->arena);
    return true;
}

static void gecs__test_context_free(void* user) {
    gecs__AllocatorContext* context = user;
    if (context->world != NULL) {
        gecs_World_free(context->world);
        context->world = NULL;
    }
    if (context->arena_initialized) {
        garena_destroy(&context->arena);
        context->arena_initialized = false;
    }
}

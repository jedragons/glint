#include <glint/alloc.h>
#include <glint/arena.h>

#include <mewunit.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct gecs__TestCounts {
    size_t allocations;
    size_t frees;
    bool fail;
} gecs__TestCounts;

static void *gecs__test_count_alloc(void *data, size_t size) {
    gecs__TestCounts *counts = data;
    if (counts->fail) {
        return NULL;
    }
    counts->allocations++;
    return malloc(size);
}

static void gecs__test_count_free(void *data, void *ptr) {
    gecs__TestCounts *counts = data;
    counts->frees++;
    free(ptr);
}

static void *gecs__test_count_calloc(void *data, size_t count, size_t size) {
    gecs__TestCounts *counts = data;
    counts->allocations++;
    return calloc(count, size);
}

static void *gecs__test_count_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gecs__TestCounts *counts = data;
    (void)old_size;
    if (!ptr) {
        counts->allocations++;
    }
    return realloc(ptr, new_size);
}

TEST(allocator_test, {
    static const galloc_FunctionTable gecs__test_table = {gecs__test_count_alloc, gecs__test_count_free, gecs__test_count_calloc, gecs__test_count_realloc};
    gecs__TestCounts counts = {0};
    galloc_Allocator backing = {&counts, &gecs__test_table};
    garena_Arena arena;
    garena_init(&arena, backing, 64);
    galloc_Allocator alloc = garena_allocator(&arena);
    char *first = galloc_alloc(alloc, 17);
    char *second = galloc_alloc(alloc, 80);
    mewassert("Test expectation failed", first && second);
    mewassert("Test expectation failed", (uintptr_t)first % _Alignof(max_align_t) == 0);
    mewassert("Test expectation failed", (uintptr_t)second % _Alignof(max_align_t) == 0);
    mewassert("Test expectation failed", counts.allocations == 2);
    memcpy(first, "arena", 6);
    char *larger = galloc_realloc(alloc, first, 17, 200);
    mewassert("Test expectation failed", larger && !memcmp(larger, "arena", 6));
    mewassert("Test expectation failed", counts.allocations == 3);
    mewassert("Test expectation failed", !galloc_calloc(alloc, SIZE_MAX, 2));
    mewassert("overflow is rejected", !galloc_alloc(alloc, SIZE_MAX));
    counts.fail = true;
    mewassert("reallocation failure is reported", !galloc_realloc(alloc, larger, 200, 10000));
    mewassert("failed reallocation preserves old data", !memcmp(larger, "arena", 6));
    counts.fail = false;
    garena_reset(&arena);
    mewassert("Test expectation failed", galloc_alloc(alloc, 17) == first);
    mewassert("Test expectation failed", galloc_alloc(alloc, 80) == second);
    mewassert("Test expectation failed", galloc_alloc(alloc, 200) == larger);
    mewassert("Test expectation failed", counts.allocations == 3);
    garena_reset(&arena);
    char *zeroed = galloc_calloc(alloc, 4, 4);
    for (size_t i = 0; i < 16; ++i) {
        mewassert("Test expectation failed", zeroed[i] == 0);
    }
    garena_destroy(backing, &arena);
    mewassert("Test expectation failed", counts.frees == counts.allocations);
})

mewtest_main();

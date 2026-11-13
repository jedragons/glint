#include <glint/alloc.h>

#include <stdint.h>
#include <stdlib.h>

void *galloc_alloc(galloc_Allocator alloc, size_t bytes) {
    return alloc.ftable->alloc(alloc.data, bytes);
}

void galloc_free(galloc_Allocator alloc, void *ptr) {
    if (ptr) {
        alloc.ftable->free(alloc.data, ptr);
    }
}

void *galloc_calloc(galloc_Allocator alloc, size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        return NULL;
    }
    return alloc.ftable->calloc(alloc.data, count, size);
}

void *galloc_realloc(galloc_Allocator alloc, void *ptr, size_t old_size, size_t new_size) {
    return alloc.ftable->realloc(alloc.data, ptr, old_size, new_size);
}

static void *galloc__malloc_alloc(void *data, size_t bytes) {
    (void)data;
    return malloc(bytes);
}

static void galloc__malloc_free(void *data, void *ptr) {
    (void)data;
    free(ptr);
}

static void *galloc__malloc_calloc(void *data, size_t count, size_t size) {
    (void)data;
    return calloc(count, size);
}

static void *galloc__malloc_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    (void)data;
    (void)old_size;
    return realloc(ptr, new_size);
}

galloc_Allocator galloc_malloc_allocator(void) {
    static const galloc_FunctionTable galloc__malloc_table = {
        .alloc = galloc__malloc_alloc,
        .free = galloc__malloc_free,
        .calloc = galloc__malloc_calloc,
        .realloc = galloc__malloc_realloc,
    };
    return (galloc_Allocator) {.data = NULL, .ftable = &galloc__malloc_table};
}

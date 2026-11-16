#include "./arena.h"

#include <glint/config.h>

#include <stdint.h>
#include <string.h>

#if GLINT_COMPILER_MSVC
// MSVC's C headers do not define C11 max_align_t. Its fundamental types are
// covered by this union; the backing allocator must provide their alignment.
typedef union garena__MaxAlign {
    long double floating;
    long long integer;
    void* pointer;
    void (*function)(void);
} garena__MaxAlign;
#else
typedef max_align_t garena__MaxAlign;
#endif

struct garena__Block {
    garena__Block* next;
    size_t used;
    size_t capacity;
    garena__MaxAlign data[];
};

#define GARENA_DEFAULT_BLOCK_SIZE 8192

static void* garena__allocate(garena_Arena* arena, size_t size);
static void* garena__alloc_callback(void* data, size_t size);
static void garena__free_callback(void* data, void* ptr);
static void* garena__calloc_callback(void* data, size_t count, size_t size);
static void* garena__realloc_callback(void* data, void* ptr, size_t old_size, size_t new_size);

void garena_init(garena_Arena* arena, galloc_Allocator backing, size_t block_size) {
    *arena = (garena_Arena) {
        .backing = backing,
        .block_size = block_size != 0 ? block_size : GARENA_DEFAULT_BLOCK_SIZE,
    };
}

galloc_Allocator garena_allocator(garena_Arena* arena) {
    static const galloc_FunctionTable garena__allocator_table = {
        .alloc = garena__alloc_callback,
        .free = garena__free_callback,
        .calloc = garena__calloc_callback,
        .realloc = garena__realloc_callback,
    };

    return (galloc_Allocator) {
        .data = arena,
        .ftable = &garena__allocator_table,
    };
}

void garena_reset(garena_Arena* arena) {
    for (garena__Block* block = arena->blocks; block != NULL; block = block->next) {
        block->used = 0;
    }

    arena->current = arena->blocks;
}

void garena_destroy(garena_Arena* arena) {
    garena__Block* block = arena->blocks;

    while (block != NULL) {
        garena__Block* next = block->next;
        galloc_free(arena->backing, block);
        block = next;
    }

    arena->blocks = NULL;
    arena->current = NULL;
}

static void* garena__allocate(garena_Arena* arena, size_t size) {
    const size_t align = _Alignof(garena__MaxAlign);
    garena__Block* block = arena->current != NULL ? arena->current : arena->blocks;
    garena__Block* previous = NULL;

    if (size == 0) {
        size = 1;
    }

    if (size > SIZE_MAX - (align - 1)) {
        return NULL;
    }

    size = ((size + align - 1) / align) * align;
    while (block != NULL) {
        if (block->capacity - block->used >= size) {
            void* result = (uint8_t*)block->data + block->used;
            block->used += size;
            arena->current = block;
            return result;
        }
        previous = block;
        block = block->next;
    }

    size_t capacity = size > arena->block_size != 0 ? size : arena->block_size;

    if (capacity > SIZE_MAX - sizeof(*block)) {
        return NULL;
    }

    block = galloc_alloc(arena->backing, sizeof(*block) + capacity);

    if (block == NULL) {
        return NULL;
    }

    *block = (garena__Block) {.used = size, .capacity = capacity};

    if (previous != NULL) {
        previous->next = block;
    } else if (arena->blocks != NULL) {
        garena__Block* last = arena->blocks;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = block;
    } else {
        arena->blocks = block;
    }

    arena->current = block;

    return block->data;
}

static void* garena__alloc_callback(void* data, size_t size) {
    garena_Arena* arena = data;
    return garena__allocate(arena, size);
}

static void garena__free_callback(void* data, void* ptr) {
    (void)data;
    (void)ptr;
}

static void* garena__calloc_callback(void* data, size_t count, size_t size) {
    garena_Arena* arena = data;

    if (size > 0 && count > SIZE_MAX / size) {
        return NULL;
    }

    size_t bytes = count * size;
    void* result = garena__allocate(arena, bytes);
    if (result != NULL) {
        memset(result, 0, bytes);
    }

    return result;
}

static void* garena__realloc_callback(void* data, void* ptr, size_t old_size, size_t new_size) {
    garena_Arena* arena = data;
    if (new_size <= old_size && ptr != NULL) {
        return ptr;
    }

    void* result = garena__allocate(arena, new_size);
    if (result != NULL && ptr != NULL) {
        memcpy(result, ptr, old_size < new_size ? old_size : new_size);
    }

    return result;
}

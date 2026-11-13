#include <glint/arena.h>

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
/* MSVC's C headers do not define C11 max_align_t. Its fundamental types are
 * covered by this union; the backing allocator must provide their alignment. */
typedef union garena__MaxAlign {
    long double floating;
    long long integer;
    void *pointer;
    void (*function)(void);
} garena__MaxAlign;
#else
typedef max_align_t garena__MaxAlign;
#endif

struct garena__Block {
    garena__Block *next;
    size_t used;
    size_t capacity;
    garena__MaxAlign data[];
};

void garena_init(garena_Arena *arena, galloc_Allocator backing, size_t block_size) {
    *arena = (garena_Arena) {
        .backing = backing,
        .block_size = block_size ? block_size : 8192,
    };
}

static void *garena__allocate(galloc_Allocator backing, garena_Arena *arena, size_t size) {
    size_t align = _Alignof(garena__MaxAlign);
    garena__Block *block = arena->current ? arena->current : arena->blocks;
    garena__Block *previous = NULL;
    if (size == 0) {
        size = 1;
    }
    if (size > SIZE_MAX - (align - 1)) {
        return NULL;
    }
    size = ((size + align - 1) / align) * align;
    while (block) {
        if (block->capacity - block->used >= size) {
            void *result = (unsigned char *)block->data + block->used;
            block->used += size;
            arena->current = block;
            return result;
        }
        previous = block;
        block = block->next;
    }
    size_t capacity = size > arena->block_size ? size : arena->block_size;
    if (capacity > SIZE_MAX - sizeof(*block)) {
        return NULL;
    }
    block = galloc_alloc(backing, sizeof(*block) + capacity);
    if (!block) {
        return NULL;
    }
    *block = (garena__Block) {.used = size, .capacity = capacity};
    if (previous) {
        previous->next = block;
    } else if (arena->blocks) {
        /* current can only be NULL for an empty or freshly reset arena. */
        garena__Block *last = arena->blocks;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
    } else {
        arena->blocks = block;
    }
    arena->current = block;
    return block->data;
}

static void *garena__alloc_callback(void *data, size_t size) {
    garena_Arena *arena = data;
    return garena__allocate(arena->backing, arena, size);
}

static void garena__free_callback(void *data, void *ptr) {
    (void)data;
    (void)ptr;
}

static void *garena__calloc_callback(void *data, size_t count, size_t size) {
    garena_Arena *arena = data;
    if (size && count > SIZE_MAX / size) {
        return NULL;
    }
    size_t bytes = count * size;
    void *result = garena__allocate(arena->backing, arena, bytes);
    if (result) {
        memset(result, 0, bytes);
    }
    return result;
}

static void *garena__realloc_callback(void *data, void *ptr, size_t old_size, size_t new_size) {
    garena_Arena *arena = data;
    if (new_size <= old_size && ptr) {
        return ptr;
    }
    void *result = garena__allocate(arena->backing, arena, new_size);
    if (result && ptr) {
        memcpy(result, ptr, old_size < new_size ? old_size : new_size);
    }
    return result;
}

galloc_Allocator garena_allocator(garena_Arena *arena) {
    static const galloc_FunctionTable garena__allocator_table = {
        .alloc = garena__alloc_callback,
        .free = garena__free_callback,
        .calloc = garena__calloc_callback,
        .realloc = garena__realloc_callback,
    };
    return (galloc_Allocator) {.data = arena, .ftable = &garena__allocator_table};
}

void garena_reset(garena_Arena *arena) {
    for (garena__Block *block = arena->blocks; block; block = block->next) {
        block->used = 0;
    }
    arena->current = arena->blocks;
}

void garena_destroy(galloc_Allocator backing, garena_Arena *arena) {
    garena__Block *block = arena->blocks;
    while (block) {
        garena__Block *next = block->next;
        galloc_free(backing, block);
        block = next;
    }
    arena->blocks = NULL;
    arena->current = NULL;
}

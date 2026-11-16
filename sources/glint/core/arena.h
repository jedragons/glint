#pragma once

#include <glint/core/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct garena__Block garena__Block;

/// Memory arena (pool allocator)
typedef struct garena_Arena {
    galloc_Allocator backing;
    garena__Block* blocks;
    garena__Block* current;
    size_t block_size;
} garena_Arena;

/// Initialize an arena; its backing allocator must outlive it.
void garena_init(garena_Arena* arena, galloc_Allocator backing, size_t block_size);

/// Borrow the arena as an allocator. The arena must outlive this value.
galloc_Allocator garena_allocator(garena_Arena* arena);

/// Invalidate all allocations and retain storage for reuse.
void garena_reset(garena_Arena* arena);

/// Free all memory allocated by arena
void garena_destroy(garena_Arena* arena);

#ifdef __cplusplus
}
#endif

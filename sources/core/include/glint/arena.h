#ifndef GARENA_ARENA_H
#define GARENA_ARENA_H

#include <glint/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct garena__Block garena__Block;

typedef struct garena_Arena {
    galloc_Allocator backing;
    garena__Block *blocks;
    garena__Block *current;
    size_t block_size;
} garena_Arena;

/* The backing allocator and its state must outlive the arena. */
void garena_init(garena_Arena *arena, galloc_Allocator backing, size_t block_size);
galloc_Allocator garena_allocator(garena_Arena *arena);
/* Reset invalidates all allocations but retains blocks for reuse. */
void garena_reset(garena_Arena *arena);
/* Pass the same backing allocator used to initialize the arena. */
void garena_destroy(galloc_Allocator backing, garena_Arena *arena);

#ifdef __cplusplus
}
#endif
#endif

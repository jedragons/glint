#ifndef GALLOC_ALLOC_H
#define GALLOC_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *galloc_AllocFn(void *data, size_t bytes);
typedef void galloc_FreeFn(void *data, void *ptr);
typedef void *galloc_CallocFn(void *data, size_t count, size_t size);
typedef void *galloc_ReallocFn(void *data, void *ptr, size_t old_size, size_t new_size);

typedef struct galloc_FunctionTable {
    galloc_AllocFn *alloc;
    galloc_FreeFn *free;
    galloc_CallocFn *calloc;
    galloc_ReallocFn *realloc;
} galloc_FunctionTable;

typedef struct galloc_Allocator {
    void *data;
    const galloc_FunctionTable *ftable;
} galloc_Allocator;

void *galloc_alloc(galloc_Allocator alloc, size_t bytes);
void galloc_free(galloc_Allocator alloc, void *ptr);
void *galloc_calloc(galloc_Allocator alloc, size_t count, size_t size);
void *galloc_realloc(galloc_Allocator alloc, void *ptr, size_t old_size, size_t new_size);
galloc_Allocator galloc_malloc_allocator(void);

#ifdef __cplusplus
}
#endif
#endif

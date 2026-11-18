/// @file
/// Assets cached by (type, path), with per-type load/reload/unload callbacks.
///
/// # Ownership
///
/// Use one allocator for the cache's lifetime.
///
/// # Callbacks
///
/// Callbacks may access other assets. Access to an asset currently loading or
/// reloading fails, including cyclic loads. Cache operations are not thread-safe.

#pragma once

#include <glint/core/alloc.h>
#include <glint/platform/watcher.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gasset_Cache gasset_Cache;

typedef bool (*gasset_LoadFn)(galloc_Allocator alloc, void* instance, const char* path, void* user);

typedef bool (*gasset_ReloadFn)(galloc_Allocator alloc, void* instance, const char* path, void* user);

typedef void (*gasset_UnloadFn)(galloc_Allocator alloc, void* instance, void* user);

gasset_Cache* gasset_Cache_create(galloc_Allocator alloc);

void gasset_Cache_destroy(galloc_Allocator alloc, gasset_Cache* cache);

/// True while callbacks, polling or teardown prevent unregistering types.
bool gasset_Cache_busy(const gasset_Cache* cache);

bool gasset_Cache_register(
    galloc_Allocator alloc,
    gasset_Cache* cache,
    const char* type,
    size_t instance_size,
    gasset_LoadFn load,
    gasset_ReloadFn reload,
    gasset_UnloadFn unload,
    void* user
);

/// Unload a type's entries in reverse load order and unregister it.
bool gasset_Cache_unregister(galloc_Allocator alloc, gasset_Cache* cache, const char* type);

/// Loads by (type, path); cached assets return the same instance again.
void* gasset_Cache_load(galloc_Allocator alloc, gasset_Cache* cache, const char* type, const char* path);

/// Peeks without loading.
void* gasset_Cache_get(const gasset_Cache* cache, const char* type, const char* path);

bool gasset_Cache_contains(const gasset_Cache* cache, const char* type, const char* path);

/// Reloads a cached asset into the same instance.
bool gasset_Cache_reload(galloc_Allocator alloc, gasset_Cache* cache, const char* type, const char* path);

bool gasset_Cache_watch(
    galloc_Allocator alloc,
    gasset_Cache* cache,
    const char* type,
    const char* path,
    const char* native_path
);

/// Polls the cache's watcher and reloads assets whose native files changed.
/// Recursive polls on the same cache are ignored.
void gasset_Cache_poll(galloc_Allocator alloc, gasset_Cache* cache);

#ifdef __cplusplus
}
#endif

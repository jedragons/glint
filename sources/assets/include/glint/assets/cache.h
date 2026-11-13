#ifndef GASSET_ASSETS_CACHE_H_
#define GASSET_ASSETS_CACHE_H_

#include <glint/alloc.h>
#include <glint/assets/file_watcher.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Typed asset cache: named types with load/reload/unload callbacks and a
 * (type, path) keyed cache of loaded instances. The same allocator must be
 * used for every operation on a cache and must outlive it. Callbacks may load,
 * reload, watch, or register other assets/types. A load, get, or reload of the
 * same (type, path) while it is loading or reloading fails. Cyclic loads are
 * therefore rejected. Calls are not thread-safe. */

typedef struct gasset_Cache gasset_Cache;

/* Instance memory is zeroed before load. A nonzero return means success;
 * failed loads are not cached and may be retried. `user` is the pointer given
 * at type registration. */
typedef int (*gasset_LoadFn)(galloc_Allocator alloc, void *instance, const char *path, void *user);
/* Reloads in place. A nonzero return keeps the reloaded instance; a zero
 * return leaves the previous contents untouched when the type provides its
 * own reload. */
typedef int (*gasset_ReloadFn)(galloc_Allocator alloc, void *instance, const char *path, void *user);
/* Releases everything owned by the instance; called before its memory is
 * freed. */
typedef void (*gasset_UnloadFn)(galloc_Allocator alloc, void *instance, void *user);

/* Creates a cache with its own file watcher. Destroy unloads every cached
 * asset in reverse load order and releases the watcher. Destroy is ignored
 * during a callback or poll, including recursive destroy during teardown; call
 * it again after the operation completes. Cache operations during teardown fail. */
gasset_Cache *gasset_Cache_create(galloc_Allocator alloc);
void gasset_Cache_destroy(galloc_Allocator alloc, gasset_Cache *cache);

/* Registers an asset type; duplicate names are rejected. A NULL reload falls
 * back to unload followed by load, evicting the entry when that load fails. */
bool gasset_Cache_register(
    galloc_Allocator alloc, gasset_Cache *cache, const char *type, size_t instance_size,
    gasset_LoadFn load, gasset_ReloadFn reload, gasset_UnloadFn unload, void *user
);
/* Loads by (type, path); cached assets return the same instance again.
 * Returns NULL for unknown types, failures, or allocation errors. */
void *gasset_Cache_load(galloc_Allocator alloc, gasset_Cache *cache, const char *type, const char *path);
/* Peeks without loading; NULL when the asset is not cached or the type is
 * unknown. */
void *gasset_Cache_get(const gasset_Cache *cache, const char *type, const char *path);
bool gasset_Cache_contains(const gasset_Cache *cache, const char *type, const char *path);
/* Reloads a cached asset into the same instance; false when the type is
 * unknown, the asset is not cached, or the reload failed. A failed fallback
 * reload evicts the entry. */
bool gasset_Cache_reload(galloc_Allocator alloc, gasset_Cache *cache, const char *type, const char *path);

/* Hot reload: maps a native file to an asset path so that a poll observing a
 * change to the file reloads the asset. The same native file may back several
 * assets; each is reloaded. */
bool gasset_Cache_watch(
    galloc_Allocator alloc, gasset_Cache *cache, const char *type, const char *path, const char *native_path
);
/* Polls the cache's watcher and reloads assets whose native files changed.
 * Recursive polls on the same cache are ignored. */
void gasset_Cache_poll(galloc_Allocator alloc, gasset_Cache *cache);

#ifdef __cplusplus
}
#endif

#endif // GASSET_ASSETS_CACHE_H_

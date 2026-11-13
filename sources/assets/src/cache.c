#include <glint/assets/cache.h>

#include <stdint.h>
#include <string.h>

typedef struct gasset__AssetCacheType {
    char *name;
    size_t instance_size;
    gasset_LoadFn load;
    gasset_ReloadFn reload;
    gasset_UnloadFn unload;
    void *user;
} gasset__AssetCacheType;

typedef struct gasset__AssetCacheEntry {
    size_t type;
    char *path;
    void *instance;
} gasset__AssetCacheEntry;

typedef struct gasset__AssetCacheWatch {
    size_t type;
    char *path;
    char *native_path;
} gasset__AssetCacheWatch;

typedef struct gasset__AssetCacheOperation {
    size_t type;
    const char *path;
    struct gasset__AssetCacheOperation *previous;
} gasset__AssetCacheOperation;

struct gasset_Cache {
    gasset__AssetCacheType *types;
    size_t type_count, type_capacity;
    gasset__AssetCacheEntry *entries;
    size_t entry_count, entry_capacity;
    gasset__AssetCacheWatch *watched;
    size_t watched_count, watched_capacity;
    gwatch_Watcher *watcher;
    gasset__AssetCacheOperation *active;
    bool destroying, polling;
};

typedef struct gasset__AssetCachePoll {
    galloc_Allocator alloc;
    gasset_Cache *cache;
} gasset__AssetCachePoll;

static char *gasset__cache_cache_copy_string(galloc_Allocator alloc, const char *text) {
    size_t length = strlen(text);
    char *copy = galloc_alloc(alloc, length + 1);
    if (copy) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static bool gasset__cache_cache_grow(galloc_Allocator alloc, void **items, size_t count, size_t *capacity, size_t element_size) {
    size_t grown_capacity = *capacity ? *capacity * 2 : 4;
    void *grown = NULL;
    if (grown_capacity > count && grown_capacity <= SIZE_MAX / element_size) {
        grown = galloc_realloc(alloc, *items, *capacity * element_size, grown_capacity * element_size);
    }
    if (!grown) {
        return false;
    }
    *items = grown;
    *capacity = grown_capacity;
    return true;
}

static size_t gasset__cache_cache_find_type(const gasset_Cache *cache, const char *type) {
    if (!cache || !type) {
        return SIZE_MAX;
    }
    for (size_t i = 0; i < cache->type_count; i++) {
        if (strcmp(cache->types[i].name, type) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static gasset__AssetCacheEntry *gasset__cache_cache_find_entry(gasset_Cache *cache, size_t type, const char *path) {
    if (!path) {
        return NULL;
    }
    for (size_t i = 0; i < cache->entry_count; i++) {
        gasset__AssetCacheEntry *entry = &cache->entries[i];
        if (entry->type == type && strcmp(entry->path, path) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool gasset__cache_cache_is_active(const gasset_Cache *cache, size_t type, const char *path) {
    for (const gasset__AssetCacheOperation *op = cache->active; op; op = op->previous) {
        if (op->type == type && strcmp(op->path, path) == 0) {
            return true;
        }
    }
    return false;
}

gasset_Cache *gasset_Cache_create(galloc_Allocator alloc) {
    gasset_Cache *cache = galloc_calloc(alloc, 1, sizeof(*cache));
    if (!cache) {
        return NULL;
    }
    cache->watcher = gwatch_create(alloc);
    if (!cache->watcher) {
        galloc_free(alloc, cache);
        return NULL;
    }
    return cache;
}

void gasset_Cache_destroy(galloc_Allocator alloc, gasset_Cache *cache) {
    if (!cache || cache->destroying || cache->active || cache->polling) {
        return;
    }
    cache->destroying = true;
    /* Reverse load order so dependent assets tear down before their sources. */
    for (size_t i = cache->entry_count; i > 0; i--) {
        gasset__AssetCacheEntry *entry = &cache->entries[i - 1];
        const gasset__AssetCacheType *type = &cache->types[entry->type];
        if (type->unload) {
            type->unload(alloc, entry->instance, type->user);
        }
        galloc_free(alloc, entry->instance);
        galloc_free(alloc, entry->path);
    }
    for (size_t i = 0; i < cache->type_count; i++) {
        galloc_free(alloc, cache->types[i].name);
    }
    for (size_t i = 0; i < cache->watched_count; i++) {
        galloc_free(alloc, cache->watched[i].path);
        galloc_free(alloc, cache->watched[i].native_path);
    }
    gwatch_destroy(alloc, cache->watcher);
    galloc_free(alloc, cache->types);
    galloc_free(alloc, cache->entries);
    galloc_free(alloc, cache->watched);
    galloc_free(alloc, cache);
}

bool gasset_Cache_register(
    galloc_Allocator alloc, gasset_Cache *cache, const char *type, size_t instance_size,
    gasset_LoadFn load, gasset_ReloadFn reload, gasset_UnloadFn unload, void *user
) {
    if (!cache || cache->destroying || !type || !*type || !instance_size || !load) {
        return false;
    }
    if (gasset__cache_cache_find_type(cache, type) != SIZE_MAX) {
        return false;
    }
    if (cache->type_count == cache->type_capacity && !gasset__cache_cache_grow(alloc, (void **)&cache->types, cache->type_count,
                                                                 &cache->type_capacity, sizeof(*cache->types))) {
        return false;
    }
    char *name = gasset__cache_cache_copy_string(alloc, type);
    if (!name) {
        return false;
    }
    gasset__AssetCacheType registered = {
        .name = name,
        .instance_size = instance_size,
        .load = load,
        .reload = reload,
        .unload = unload,
        .user = user,
    };
    cache->types[cache->type_count++] = registered;
    return true;
}

void *gasset_Cache_load(galloc_Allocator alloc, gasset_Cache *cache, const char *type, const char *path) {
    size_t index = gasset__cache_cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || !path || gasset__cache_cache_is_active(cache, index, path)) {
        return NULL;
    }
    gasset__AssetCacheEntry *cached = gasset__cache_cache_find_entry(cache, index, path);
    if (cached) {
        return cached->instance;
    }
    gasset__AssetCacheType registered = cache->types[index];
    char *copy = gasset__cache_cache_copy_string(alloc, path);
    if (!copy) {
        return NULL;
    }
    void *instance = galloc_calloc(alloc, 1, registered.instance_size);
    if (!instance) {
        galloc_free(alloc, copy);
        return NULL;
    }
    gasset__AssetCacheOperation op = {.type = index, .path = copy, .previous = cache->active};
    cache->active = &op;
    if (!registered.load(alloc, instance, copy, registered.user)) {
        cache->active = op.previous;
        galloc_free(alloc, copy);
        galloc_free(alloc, instance);
        return NULL;
    }
    if (cache->entry_count == cache->entry_capacity &&
        !gasset__cache_cache_grow(alloc, (void **)&cache->entries, cache->entry_count, &cache->entry_capacity,
                    sizeof(*cache->entries))) {
        if (registered.unload) {
            registered.unload(alloc, instance, registered.user);
        }
        cache->active = op.previous;
        galloc_free(alloc, copy);
        galloc_free(alloc, instance);
        return NULL;
    }
    gasset__AssetCacheEntry entry = {.type = index, .path = copy, .instance = instance};
    cache->entries[cache->entry_count++] = entry;
    cache->active = op.previous;
    return instance;
}

void *gasset_Cache_get(const gasset_Cache *cache, const char *type, const char *path) {
    size_t index = gasset__cache_cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || !path || gasset__cache_cache_is_active(cache, index, path)) {
        return NULL;
    }
    gasset__AssetCacheEntry *entry = gasset__cache_cache_find_entry((gasset_Cache *)cache, index, path);
    return entry ? entry->instance : NULL;
}

bool gasset_Cache_contains(const gasset_Cache *cache, const char *type, const char *path) {
    return gasset_Cache_get(cache, type, path) != NULL;
}

/* Reloads one entry. A type-defined reload keeps the old contents on failure;
 * the unload+load fallback evicts the entry when the new load fails. */
static bool gasset__cache_cache_reload_entry(galloc_Allocator alloc, gasset_Cache *cache, gasset__AssetCacheEntry *entry) {
    size_t index = entry->type;
    char *path = entry->path;
    void *instance = entry->instance;
    if (gasset__cache_cache_is_active(cache, index, path)) {
        return false;
    }
    gasset__AssetCacheType type = cache->types[index];
    gasset__AssetCacheOperation op = {.type = index, .path = path, .previous = cache->active};
    cache->active = &op;
    if (type.reload) {
        bool result = type.reload(alloc, instance, path, type.user) != 0;
        cache->active = op.previous;
        return result;
    }
    if (type.unload) {
        type.unload(alloc, instance, type.user);
    }
    memset(instance, 0, type.instance_size);
    if (!type.load(alloc, instance, path, type.user)) {
        entry = gasset__cache_cache_find_entry(cache, index, path);
        size_t position = (size_t)(entry - cache->entries);
        memmove(cache->entries + position, cache->entries + position + 1,
                (cache->entry_count - position - 1) * sizeof(*cache->entries));
        cache->entry_count--;
        cache->active = op.previous;
        galloc_free(alloc, path);
        galloc_free(alloc, instance);
        return false;
    }
    cache->active = op.previous;
    return true;
}

bool gasset_Cache_reload(galloc_Allocator alloc, gasset_Cache *cache, const char *type, const char *path) {
    size_t index = gasset__cache_cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || !path) {
        return false;
    }
    gasset__AssetCacheEntry *entry = gasset__cache_cache_find_entry(cache, index, path);
    if (!entry) {
        return false;
    }
    return gasset__cache_cache_reload_entry(alloc, cache, entry);
}

bool gasset_Cache_watch(
    galloc_Allocator alloc, gasset_Cache *cache, const char *type, const char *path, const char *native_path
) {
    size_t index = gasset__cache_cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || !path || !native_path || !*native_path) {
        return false;
    }
    for (size_t i = 0; i < cache->watched_count; i++) {
        gasset__AssetCacheWatch *watched = &cache->watched[i];
        if (watched->type == index && strcmp(watched->path, path) == 0) {
            if (!gwatch_add(alloc, cache->watcher, native_path)) {
                return false;
            }
            char *copy = gasset__cache_cache_copy_string(alloc, native_path);
            if (!copy) {
                return false;
            }
            galloc_free(alloc, watched->native_path);
            watched->native_path = copy;
            return true;
        }
    }
    if (cache->watched_count == cache->watched_capacity &&
        !gasset__cache_cache_grow(alloc, (void **)&cache->watched, cache->watched_count, &cache->watched_capacity,
                    sizeof(*cache->watched))) {
        return false;
    }
    char *path_copy = gasset__cache_cache_copy_string(alloc, path);
    char *native_copy = path_copy ? gasset__cache_cache_copy_string(alloc, native_path) : NULL;
    if (!native_copy) {
        galloc_free(alloc, path_copy);
        return false;
    }
    gasset__AssetCacheWatch watched = {.type = index, .path = path_copy, .native_path = native_copy};
    cache->watched[cache->watched_count++] = watched;
    if (!gwatch_add(alloc, cache->watcher, native_path)) {
        cache->watched_count--;
        galloc_free(alloc, path_copy);
        galloc_free(alloc, native_copy);
        return false;
    }
    return true;
}

static void gasset__cache_cache_asset_changed(const char *native_path, void *user) {
    gasset__AssetCachePoll *poll = user;
    gasset_Cache *cache = poll->cache;
    size_t count = cache->watched_count;
    for (size_t i = 0; i < count; i++) {
        const gasset__AssetCacheWatch *watched = &cache->watched[i];
        if (strcmp(watched->native_path, native_path) != 0) {
            continue;
        }
        gasset__AssetCacheEntry *entry = gasset__cache_cache_find_entry(cache, watched->type, watched->path);
        if (entry) {
            /* Type-defined reload failures retain the entry; fallback failures evict it. */
            gasset__cache_cache_reload_entry(poll->alloc, cache, entry);
        }
    }
}

void gasset_Cache_poll(galloc_Allocator alloc, gasset_Cache *cache) {
    if (!cache || cache->destroying || cache->polling || !cache->watcher) {
        return;
    }
    gasset__AssetCachePoll poll = {.alloc = alloc, .cache = cache};
    cache->polling = true;
    gwatch_poll(cache->watcher, gasset__cache_cache_asset_changed, &poll);
    cache->polling = false;
}

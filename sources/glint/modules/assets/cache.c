#include "./cache.h"

#include <stdint.h>
#include <string.h>

typedef struct gasset__AssetCacheType {
    char* name;
    size_t instance_size;
    gasset_LoadFn load;
    gasset_ReloadFn reload;
    gasset_UnloadFn unload;
    void* user;
} gasset__AssetCacheType;

typedef struct gasset__AssetCacheEntry {
    size_t type;
    char* path;
    void* instance;
} gasset__AssetCacheEntry;

typedef struct gasset__AssetCacheWatch {
    size_t type;
    char* path;
    char* native_path;
} gasset__AssetCacheWatch;

typedef struct gasset__AssetCacheOperation {
    size_t type;
    const char* path;
    struct gasset__AssetCacheOperation* previous;
} gasset__AssetCacheOperation;

struct gasset_Cache {
    gasset__AssetCacheType* types;
    size_t type_count;
    size_t type_capacity;
    gasset__AssetCacheEntry* entries;
    size_t entry_count;
    size_t entry_capacity;
    gasset__AssetCacheWatch* watched;
    size_t watched_count;
    size_t watched_capacity;
    gwatch_Watcher* watcher;
    gasset__AssetCacheOperation* active;
    bool destroying;
    bool polling;
};

typedef struct gasset__AssetCachePoll {
    galloc_Allocator alloc;
    gasset_Cache* cache;
} gasset__AssetCachePoll;

static char* gasset__cache_cache_copy_string(galloc_Allocator alloc, const char* text);

static bool gasset__cache_cache_grow(
    galloc_Allocator alloc,
    void** items,
    size_t count,
    size_t* capacity,
    size_t element_size
);

static size_t gasset__cache_find_type(const gasset_Cache* cache, const char* type);

static gasset__AssetCacheEntry* gasset__cache_find_entry(gasset_Cache* cache, size_t type, const char* path);

static bool gasset__cache_is_active(const gasset_Cache* cache, size_t type, const char* path);

static bool gasset__cache_reload_entry(galloc_Allocator alloc, gasset_Cache* cache, gasset__AssetCacheEntry* entry);

static void gasset__cache_asset_changed(const char* native_path, void* user);

gasset_Cache* gasset_Cache_create(galloc_Allocator alloc) {
    gasset_Cache* cache = galloc_calloc(alloc, 1, sizeof(*cache));

    if (cache == NULL) {
        return NULL;
    }

    cache->watcher = gwatch_create(alloc);
    if (cache->watcher == NULL) {
        galloc_free(alloc, cache);
        return NULL;
    }

    return cache;
}

bool gasset_Cache_busy(const gasset_Cache* cache) {
    return cache == NULL || cache->destroying || cache->polling || cache->active != NULL;
}

void gasset_Cache_destroy(galloc_Allocator alloc, gasset_Cache* cache) {
    if (cache == NULL || cache->destroying || cache->active != NULL || cache->polling) {
        return;
    }

    cache->destroying = true;

    // Reverse load order so dependent assets tear down before their sources.
    for (size_t i = cache->entry_count; i > 0; i--) {
        gasset__AssetCacheEntry* entry = &cache->entries[i - 1];
        const gasset__AssetCacheType* type = &cache->types[entry->type];
        if (type->unload != NULL) {
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
    galloc_Allocator alloc,
    gasset_Cache* cache,
    const char* type,
    size_t instance_size,
    gasset_LoadFn load,
    gasset_ReloadFn reload,
    gasset_UnloadFn unload,
    void* user
) {
    if (cache == NULL || cache->destroying || type == NULL || *type == 0 || instance_size == 0 || load == NULL) {
        return false;
    }

    if (gasset__cache_find_type(cache, type) != SIZE_MAX) {
        return false;
    }

    if (cache->type_count == cache->type_capacity
        && !gasset__cache_cache_grow(
            alloc,
            (void**)&cache->types,
            cache->type_count,
            &cache->type_capacity,
            sizeof(*cache->types)
        )) {
        return false;
    }

    char* name = gasset__cache_cache_copy_string(alloc, type);
    if (name == NULL) {
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

bool gasset_Cache_unregister(galloc_Allocator alloc, gasset_Cache* cache, const char* name) {
    size_t type = gasset__cache_find_type(cache, name);
    if (type == SIZE_MAX || cache->destroying || cache->active != NULL || cache->polling) {
        return false;
    }

    cache->destroying = true;

    for (size_t i = cache->entry_count; i > 0; i--) {
        size_t index = i - 1;
        gasset__AssetCacheEntry* entry = &cache->entries[index];

        if (entry->type == type) {
            gasset__AssetCacheType* registered = &cache->types[type];
            if (registered->unload != NULL) {
                registered->unload(alloc, entry->instance, registered->user);
            }

            galloc_free(alloc, entry->instance);
            galloc_free(alloc, entry->path);
            memmove(entry, entry + 1, (cache->entry_count - index - 1) * sizeof(*entry));
            cache->entry_count -= 1;
        } else if (entry->type > type) {
            entry->type -= 1;
        }
    }

    for (size_t i = cache->watched_count; i > 0; i--) {
        size_t index = i - 1;
        gasset__AssetCacheWatch* watch = &cache->watched[index];

        if (watch->type == type) {
            galloc_free(alloc, watch->path);
            galloc_free(alloc, watch->native_path);
            memmove(watch, watch + 1, (cache->watched_count - index - 1) * sizeof(*watch));
            cache->watched_count--;
        } else if (watch->type > type) {
            watch->type--;
        }
    }

    galloc_free(alloc, cache->types[type].name);

    memmove(&cache->types[type], &cache->types[type + 1], (cache->type_count - type - 1) * sizeof(*cache->types));
    cache->type_count--;
    cache->destroying = false;

    return true;
}

void* gasset_Cache_load(galloc_Allocator alloc, gasset_Cache* cache, const char* type, const char* path) {
    size_t index = gasset__cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || path == NULL || gasset__cache_is_active(cache, index, path)) {
        return NULL;
    }

    gasset__AssetCacheEntry* cached = gasset__cache_find_entry(cache, index, path);
    if (cached != NULL) {
        return cached->instance;
    }

    gasset__AssetCacheType registered = cache->types[index];
    char* copy = gasset__cache_cache_copy_string(alloc, path);
    if (copy == NULL) {
        return NULL;
    }

    void* instance = galloc_calloc(alloc, 1, registered.instance_size);
    if (instance == NULL) {
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

    if (cache->entry_count == cache->entry_capacity
        && !gasset__cache_cache_grow(
            alloc,
            (void**)&cache->entries,
            cache->entry_count,
            &cache->entry_capacity,
            sizeof(*cache->entries)
        )) {
        if (registered.unload != NULL) {
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

void* gasset_Cache_get(const gasset_Cache* cache, const char* type, const char* path) {
    size_t index = gasset__cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || path == NULL || gasset__cache_is_active(cache, index, path)) {
        return NULL;
    }

    gasset__AssetCacheEntry* entry = gasset__cache_find_entry((gasset_Cache*)cache, index, path);

    return entry != NULL ? entry->instance : NULL;
}

bool gasset_Cache_contains(const gasset_Cache* cache, const char* type, const char* path) {
    return gasset_Cache_get(cache, type, path) != NULL;
}

bool gasset_Cache_reload(galloc_Allocator alloc, gasset_Cache* cache, const char* type, const char* path) {
    size_t index = gasset__cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || path == NULL) {
        return false;
    }

    gasset__AssetCacheEntry* entry = gasset__cache_find_entry(cache, index, path);
    if (entry == NULL) {
        return false;
    }

    return gasset__cache_reload_entry(alloc, cache, entry);
}

bool gasset_Cache_watch(
    galloc_Allocator alloc,
    gasset_Cache* cache,
    const char* type,
    const char* path,
    const char* native_path
) {
    size_t index = gasset__cache_find_type(cache, type);
    if (index == SIZE_MAX || cache->destroying || path == NULL || native_path == NULL || *native_path == 0) {
        return false;
    }

    for (size_t i = 0; i < cache->watched_count; i++) {
        gasset__AssetCacheWatch* watched = &cache->watched[i];
        if (watched->type == index && strcmp(watched->path, path) == 0
            && strcmp(watched->native_path, native_path) == 0) {
            return true;
        }
    }

    if (cache->watched_count == cache->watched_capacity
        && !gasset__cache_cache_grow(
            alloc,
            (void**)&cache->watched,
            cache->watched_count,
            &cache->watched_capacity,
            sizeof(*cache->watched)
        )) {
        return false;
    }

    char* path_copy = gasset__cache_cache_copy_string(alloc, path);
    char* native_copy = path_copy != NULL ? gasset__cache_cache_copy_string(alloc, native_path) : NULL;
    if (native_copy == NULL) {
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

void gasset_Cache_poll(galloc_Allocator alloc, gasset_Cache* cache) {
    if (cache == NULL || cache->destroying || cache->polling || cache->watcher == NULL) {
        return;
    }

    gasset__AssetCachePoll poll = {.alloc = alloc, .cache = cache};
    cache->polling = true;
    gwatch_poll(cache->watcher, gasset__cache_asset_changed, &poll);
    cache->polling = false;
}

static char* gasset__cache_cache_copy_string(galloc_Allocator alloc, const char* text) {
    size_t length = strlen(text);
    char* copy = galloc_alloc(alloc, length + 1);

    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }

    return copy;
}

static bool gasset__cache_cache_grow(
    galloc_Allocator alloc,
    void** items,
    size_t count,
    size_t* capacity,
    size_t element_size
) {
    size_t grown_capacity = *capacity != 0 ? *capacity * 2 : 4;
    void* grown = NULL;
    if (grown_capacity > count && grown_capacity <= SIZE_MAX / element_size) {
        grown = galloc_realloc(alloc, *items, *capacity * element_size, grown_capacity * element_size);
    }
    if (grown == NULL) {
        return false;
    }
    *items = grown;
    *capacity = grown_capacity;
    return true;
}

static size_t gasset__cache_find_type(const gasset_Cache* cache, const char* type) {
    if (cache == NULL || type == NULL) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < cache->type_count; i++) {
        if (strcmp(cache->types[i].name, type) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

static gasset__AssetCacheEntry* gasset__cache_find_entry(gasset_Cache* cache, size_t type, const char* path) {
    if (path == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < cache->entry_count; i++) {
        gasset__AssetCacheEntry* entry = &cache->entries[i];
        if (entry->type == type && strcmp(entry->path, path) == 0) {
            return entry;
        }
    }

    return NULL;
}

static bool gasset__cache_is_active(const gasset_Cache* cache, size_t type, const char* path) {
    for (const gasset__AssetCacheOperation* op = cache->active; op != NULL; op = op->previous) {
        if (op->type == type && strcmp(op->path, path) == 0) {
            return true;
        }
    }

    return false;
}

static bool gasset__cache_reload_entry(galloc_Allocator alloc, gasset_Cache* cache, gasset__AssetCacheEntry* entry) {
    size_t index = entry->type;
    char* path = entry->path;
    void* instance = entry->instance;

    if (gasset__cache_is_active(cache, index, path)) {
        return false;
    }

    gasset__AssetCacheType type = cache->types[index];
    gasset__AssetCacheOperation op = {.type = index, .path = path, .previous = cache->active};
    cache->active = &op;

    if (type.reload != NULL) {
        bool result = type.reload(alloc, instance, path, type.user);
        cache->active = op.previous;
        return result;
    }

    if (type.unload != NULL) {
        type.unload(alloc, instance, type.user);
    }

    memset(instance, 0, type.instance_size);

    if (!type.load(alloc, instance, path, type.user)) {
        entry = gasset__cache_find_entry(cache, index, path);
        size_t position = (size_t)(entry - cache->entries);

        memmove(
            cache->entries + position,
            cache->entries + position + 1,
            (cache->entry_count - position - 1) * sizeof(*cache->entries)
        );

        cache->entry_count--;
        cache->active = op.previous;
        galloc_free(alloc, path);
        galloc_free(alloc, instance);

        return false;
    }

    cache->active = op.previous;

    return true;
}

static void gasset__cache_asset_changed(const char* native_path, void* user) {
    gasset__AssetCachePoll* poll = user;
    gasset_Cache* cache = poll->cache;
    size_t count = cache->watched_count;

    for (size_t i = 0; i < count; i++) {
        const gasset__AssetCacheWatch* watched = &cache->watched[i];
        if (strcmp(watched->native_path, native_path) != 0) {
            continue;
        }

        gasset__AssetCacheEntry* entry = gasset__cache_find_entry(cache, watched->type, watched->path);
        if (entry != NULL) {
            gasset__cache_reload_entry(poll->alloc, cache, entry);
        }
    }
}

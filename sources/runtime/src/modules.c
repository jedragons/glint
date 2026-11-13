#include <glint/modules.h>
#include <glint/arena.h>

#include <stdint.h>
#include <string.h>

typedef struct gmod__ModuleEntry {
    gmod_Module module;
    bool importing;
    bool loaded;
} gmod__ModuleEntry;

typedef enum gmod__ModulePhase {
    GMOD_IDLE,
    GMOD_LOADING,
    GMOD_ACTIVE,
    GMOD_DISPATCH,
    GMOD_UNLOADING,
} gmod__ModulePhase;

struct gmod_Modules {
    galloc_Allocator alloc;
    gecs_World *world;
    gmod__ModuleEntry *entries;
    size_t count;
    size_t capacity;
    gmod__ModulePhase phase;
    size_t imports;
    garena_Arena frame_arena;
    gecs_Component frame_context;
};

static bool gmod__modules_module_allocator_matches(const gmod_Modules *modules, galloc_Allocator alloc) {
    return modules && modules->alloc.data == alloc.data && modules->alloc.ftable == alloc.ftable;
}

static char *gmod__modules_module_copy_string(galloc_Allocator alloc, const char *source) {
    if (!source) return NULL;
    size_t length = strlen(source);
    if (length == SIZE_MAX) return NULL;
    char *copy = galloc_alloc(alloc, length + 1);
    if (copy) memcpy(copy, source, length + 1);
    return copy;
}

static void gmod__modules_module_config_free(galloc_Allocator alloc, gmod_ModuleConfig *config) {
    gmod_ModuleConfigEntry *entries = (gmod_ModuleConfigEntry *)config->entries;
    for (size_t i = 0; i < config->count; i++) {
        galloc_free(alloc, (void *)entries[i].key);
        if (entries[i].kind == GMOD_STRING) galloc_free(alloc, (void *)entries[i].value.string);
    }
    galloc_free(alloc, entries);
    *config = (gmod_ModuleConfig){0};
}

const gmod_ModuleConfigEntry *gmod_ModuleConfig_get(const gmod_ModuleConfig *config, const char *key) {
    if (!config || !key || (config->count && !config->entries)) return NULL;
    for (size_t i = 0; i < config->count; i++) {
        if (config->entries[i].key && strcmp(config->entries[i].key, key) == 0) return &config->entries[i];
    }
    return NULL;
}

const gmod_ModuleConfig *gmod_Modules_config(const gmod_Modules *modules, const char *name) {
    if (!modules || !name) return NULL;
    for (size_t i = 0; i < modules->count; i++) {
        if (strcmp(modules->entries[i].module.name, name) == 0) return &modules->entries[i].module.config;
    }
    return NULL;
}

static bool gmod__modules_module_config_copy(galloc_Allocator alloc, gmod_ModuleConfig *dest, const gmod_ModuleConfig *source) {
    *dest = (gmod_ModuleConfig){0};
    if (!source->count) return true;
    if (!source->entries || source->count > SIZE_MAX / sizeof(gmod_ModuleConfigEntry)) return false;
    gmod_ModuleConfigEntry *entries = galloc_calloc(alloc, source->count, sizeof(*entries));
    if (!entries) return false;
    dest->entries = entries;
    for (size_t i = 0; i < source->count; i++) {
        const gmod_ModuleConfigEntry *item = &source->entries[i];
        if (!item->key || item->kind < GMOD_NULL || item->kind > GMOD_DOUBLE
            || (item->kind == GMOD_STRING && !item->value.string)
            || gmod_ModuleConfig_get(dest, item->key)) goto fail;
        entries[i] = *item;
        entries[i].key = gmod__modules_module_copy_string(alloc, item->key);
        if (item->kind == GMOD_STRING) entries[i].value.string = NULL;
        dest->count++;
        if (!entries[i].key) goto fail;
        if (item->kind == GMOD_STRING) {
            entries[i].value.string = gmod__modules_module_copy_string(alloc, item->value.string);
            if (!entries[i].value.string) goto fail;
        }
    }
    return true;
fail:
    gmod__modules_module_config_free(alloc, dest);
    return false;
}

gmod_Modules *gmod_Modules_new(galloc_Allocator alloc, gecs_World *world) {
    if (!world) {
        return NULL;
    }
    gmod_Modules *modules = galloc_calloc(alloc, 1, sizeof(*modules));
    if (!modules) {
        return NULL;
    }
    modules->alloc = alloc;
    modules->world = world;
    garena_init(&modules->frame_arena, alloc, 0);
    modules->frame_context = GECS_CONTEXT(alloc, world, gmod_FrameAllocator);
    if (!modules->frame_context || gecs_context_get(world, modules->frame_context)) {
        galloc_free(alloc, modules);
        return NULL;
    }
    gmod_FrameAllocator frame = {.alloc = garena_allocator(&modules->frame_arena)};
    if (!gecs_context_set(alloc, world, modules->frame_context, &frame)) {
        galloc_free(alloc, modules);
        return NULL;
    }
    return modules;
}

void gmod_Modules_free(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->imports || modules->phase == GMOD_LOADING || modules->phase == GMOD_DISPATCH ||
        modules->phase == GMOD_UNLOADING) {
        return;
    }
    gmod_Modules_unload(alloc, modules);
    for (size_t i = 0; i < modules->count; i++) {
        galloc_free(alloc, (void *)modules->entries[i].module.name);
        gmod__modules_module_config_free(alloc, &modules->entries[i].module.config);
    }
    galloc_free(alloc, modules->entries);
    gecs_context_remove(alloc, modules->world, modules->frame_context);
    garena_destroy(alloc, &modules->frame_arena);
    galloc_free(alloc, modules);
}

bool gmod_Modules_import(galloc_Allocator alloc, gmod_Modules *modules, const gmod_Module *module) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_IDLE || !module ||
        module->abi_version != GMOD_ABI_VERSION || module->struct_size < sizeof(gmod_Module) ||
        !module->name || !*module->name) {
        return false;
    }
    for (size_t i = 0; i < modules->count; i++) {
        if (strcmp(modules->entries[i].module.name, module->name) == 0) {
            return !modules->entries[i].importing;
        }
    }
    gmod_Module copy = *module;
    size_t length = strlen(copy.name);
    if (length == SIZE_MAX) {
        return false;
    }
    char *name = galloc_alloc(alloc, length + 1);
    if (!name) {
        return false;
    }
    memcpy(name, copy.name, length + 1);
    copy.name = name;
    if (!gmod__modules_module_config_copy(alloc, &copy.config, &module->config)) {
        galloc_free(alloc, name);
        return false;
    }
    if (modules->count == modules->capacity) {
        size_t capacity = modules->capacity ? modules->capacity * 2 : 8;
        gmod__ModuleEntry *entries = NULL;
        if (capacity >= modules->capacity && capacity <= SIZE_MAX / sizeof(*entries)) {
            entries = galloc_realloc(alloc, modules->entries, modules->capacity * sizeof(*entries),
                                  capacity * sizeof(*entries));
        }
        if (!entries) {
            galloc_free(alloc, name);
            gmod__modules_module_config_free(alloc, &copy.config);
            return false;
        }
        modules->entries = entries;
        modules->capacity = capacity;
    }
    size_t index = modules->count++;
    modules->entries[index] = (gmod__ModuleEntry) {.module = copy, .importing = true, .loaded = false};
    modules->imports++;
    bool success = !copy.import || copy.import(alloc, modules, modules->world, copy.user);
    modules->imports--;
    /* Nested imports may reallocate entries; refer to them by index. */
    gmod__ModuleEntry entry = modules->entries[index];
    size_t following = modules->count - index - 1;
    if (following) {
        memmove(&modules->entries[index], &modules->entries[index + 1], following * sizeof(entry));
    }
    if (!success) {
        modules->count--;
        galloc_free(alloc, (void *)entry.module.name);
        gmod__modules_module_config_free(alloc, &entry.module.config);
        return false;
    }
    entry.importing = false;
    modules->entries[modules->count - 1] = entry;
    return true;
}

bool gmod_Modules_load(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_IDLE || modules->imports) {
        return false;
    }
    modules->phase = GMOD_LOADING;
    size_t loaded = 0;
    for (; loaded < modules->count; loaded++) {
        gmod_Module *module = &modules->entries[loaded].module;
        if (module->load && !module->load(alloc, modules->world, module->user)) {
            break;
        }
        modules->entries[loaded].loaded = true;
    }
    if (loaded != modules->count) {
        modules->phase = GMOD_UNLOADING;
        while (loaded) {
            gmod_Module *module = &modules->entries[--loaded].module;
            if (module->unload) {
                module->unload(alloc, modules->world, module->user);
            }
            modules->entries[loaded].loaded = false;
        }
        modules->phase = GMOD_IDLE;
        return false;
    }
    modules->phase = GMOD_ACTIVE;
    return true;
}

bool gmod_Modules_import_active(galloc_Allocator alloc, gmod_Modules *modules, const gmod_Module *module) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) return false;
    modules->phase = GMOD_IDLE;
    bool ok = gmod_Modules_import(alloc, modules, module);
    modules->phase = GMOD_ACTIVE;
    return ok;
}

bool gmod_Modules_load_pending(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) return false;
    modules->phase = GMOD_LOADING;
    size_t first = 0;
    while (first < modules->count && modules->entries[first].loaded) first++;
    for (size_t j = first; j < modules->count; j++) {
        if (modules->entries[j].loaded) {
            modules->phase = GMOD_ACTIVE;
            return false;
        }
    }
    size_t i = first;
    for (; i < modules->count; i++) {
        gmod_Module *module = &modules->entries[i].module;
        if (module->load && !module->load(alloc, modules->world, module->user)) break;
        modules->entries[i].loaded = true;
    }
    if (i != modules->count) {
        modules->phase = GMOD_UNLOADING;
        while (i > first) {
            gmod__ModuleEntry *entry = &modules->entries[--i];
            if (entry->module.unload) entry->module.unload(alloc, modules->world, entry->module.user);
            entry->loaded = false;
        }
        modules->phase = GMOD_ACTIVE;
        return false;
    }
    modules->phase = GMOD_ACTIVE;
    return true;
}

void gmod_Modules_unload_except(galloc_Allocator alloc, gmod_Modules *modules, const char *name) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE || !name) return;
    modules->phase = GMOD_UNLOADING;
    for (size_t i = modules->count; i > 0; i--) {
        gmod__ModuleEntry *entry = &modules->entries[i - 1];
        if (entry->loaded && strcmp(entry->module.name, name)) {
            if (entry->module.unload) entry->module.unload(alloc, modules->world, entry->module.user);
            entry->loaded = false;
        }
    }
    modules->phase = GMOD_ACTIVE;
}

void gmod_Modules_unload(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return;
    }
    modules->phase = GMOD_UNLOADING;
    for (size_t i = modules->count; i > 0; i--) {
        gmod_Module *module = &modules->entries[i - 1].module;
        if (modules->entries[i - 1].loaded && module->unload) {
            module->unload(alloc, modules->world, module->user);
        }
        modules->entries[i - 1].loaded = false;
    }
    modules->phase = GMOD_IDLE;
}

void gmod_Modules_update(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return;
    }
    modules->phase = GMOD_DISPATCH;
    for (size_t i = 0; i < modules->count; i++) {
        gmod_Module *module = &modules->entries[i].module;
        if (modules->entries[i].loaded && module->update) {
            module->update(alloc, modules->world, module->user);
        }
    }
    modules->phase = GMOD_ACTIVE;
}

void gmod_Modules_render(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return;
    }
    modules->phase = GMOD_DISPATCH;
    for (size_t i = 0; i < modules->count; i++) {
        gmod_Module *module = &modules->entries[i].module;
        if (modules->entries[i].loaded && module->render) {
            module->render(alloc, modules->world, module->user);
        }
    }
    modules->phase = GMOD_ACTIVE;
}

static void gmod__modules_dispatch_named(galloc_Allocator alloc, gmod_Modules *modules, const char *name, bool only, bool render) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE || !name) return;
    modules->phase = GMOD_DISPATCH;
    for (size_t i = 0; i < modules->count; i++) {
        gmod__ModuleEntry *entry = &modules->entries[i];
        gmod_ModuleHook hook = render ? entry->module.render : entry->module.update;
        if (entry->loaded && hook && ((strcmp(entry->module.name, name) == 0) == only))
            hook(alloc, modules->world, entry->module.user);
    }
    modules->phase = GMOD_ACTIVE;
}
void gmod_Modules_update_named(galloc_Allocator a, gmod_Modules *m, const char *n, bool only) { gmod__modules_dispatch_named(a, m, n, only, false); }
void gmod_Modules_render_named(galloc_Allocator a, gmod_Modules *m, const char *n, bool only) { gmod__modules_dispatch_named(a, m, n, only, true); }

void gmod_Modules_end_frame(gmod_Modules *modules) {
    if (modules && modules->phase == GMOD_ACTIVE) {
        garena_reset(&modules->frame_arena);
    }
}

void gmod_Modules_frame(galloc_Allocator alloc, gmod_Modules *modules) {
    if (!gmod__modules_module_allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return;
    }
    gmod_Modules_update(alloc, modules);
    gmod_Modules_render(alloc, modules);
    gmod_Modules_end_frame(modules);
}

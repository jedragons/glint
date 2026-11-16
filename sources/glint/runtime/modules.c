#include <glint/runtime/modules.h>

#include <stdint.h>
#include <stdio.h>
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
    gecs_World* world;
    gmod__ModuleEntry* entries;
    size_t count;
    size_t capacity;
    gmod__ModulePhase phase;
    size_t imports;
    char error[512];
};

static bool gmod__allocator_matches(const gmod_Modules* modules, galloc_Allocator alloc);
static char* gmod__copy_string(galloc_Allocator alloc, const char* source);
static void gmod__config_free(galloc_Allocator alloc, gmod_ModuleConfig* config);
static bool gmod__config_copy(galloc_Allocator alloc, gmod_ModuleConfig* dest, const gmod_ModuleConfig* source);
static void gmod__unload_range(gmod_Modules* modules, size_t first, size_t last);
static bool gmod__load_range(gmod_Modules* modules, size_t first, gmod__ModulePhase failure_phase);
static bool gmod__preflight_unload(gmod_Modules* modules);
static bool gmod__dispatch(galloc_Allocator alloc, gmod_Modules* modules, bool render);
static bool gmod__call(gmod_Modules* modules, const gmod_Module* module, gmod_ModuleRun hook, const char* operation);

const gmod_ModuleConfigEntry* gmod_ModuleConfig_get(const gmod_ModuleConfig* config, const char* key) {
    if (config == NULL || key == NULL) {
        return NULL;
    }

    bool missing_entries = config->count > 0 && config->entries == NULL;
    if (missing_entries) {
        return NULL;
    }

    for (size_t i = 0; i < config->count; i++) {
        if (config->entries[i].key != NULL && strcmp(config->entries[i].key, key) == 0) {
            return &config->entries[i];
        }
    }

    return NULL;
}

const gmod_ModuleConfig* gmod_Modules_config(const gmod_Modules* modules, const char* name) {
    if (modules == NULL || name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < modules->count; i++) {
        if (strcmp(modules->entries[i].module.name, name) == 0) {
            return &modules->entries[i].module.config;
        }
    }

    return NULL;
}

gmod_Modules* gmod_Modules_new(gecs_World* world) {
    if (world == NULL) {
        return NULL;
    }

    galloc_Allocator alloc = glint_allocator(world);
    gmod_Modules* modules = galloc_calloc(alloc, 1, sizeof(*modules));
    if (modules == NULL) {
        return NULL;
    }

    modules->alloc = alloc;
    modules->world = world;

    return modules;
}

bool gmod_Modules_free(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc)) {
        return false;
    }

    bool lifecycle_active =
        modules->phase == GMOD_LOADING || modules->phase == GMOD_DISPATCH || modules->phase == GMOD_UNLOADING;
    if (modules->imports > 0 || lifecycle_active) {
        return false;
    }

    if (modules->phase == GMOD_ACTIVE && !gmod_Modules_unload(alloc, modules)) {
        return false;
    }

    for (size_t i = 0; i < modules->count; i++) {
        galloc_free(alloc, (void*)modules->entries[i].module.name);
        gmod__config_free(alloc, &modules->entries[i].module.config);
    }

    galloc_free(alloc, modules->entries);
    galloc_free(alloc, modules);

    return true;
}

bool gmod_Modules_import(galloc_Allocator alloc, gmod_Modules* modules, gmod_Module module) {
    bool allocator_matches = gmod__allocator_matches(modules, alloc);
    bool import_allowed = allocator_matches && modules->phase == GMOD_IDLE;
    if (!import_allowed) {
        return false;
    }

    bool abi_matches = module.abi_version == GMOD_ABI_VERSION;
    bool descriptor_complete = abi_matches && module.struct_size >= sizeof(gmod_Module);
    if (!descriptor_complete) {
        return false;
    }

    bool has_name = module.name != NULL && module.name[0] != '\0';
    if (!has_name) {
        return false;
    }

    for (size_t i = 0; i < modules->count; i++) {
        if (strcmp(modules->entries[i].module.name, module.name) == 0) {
            return !modules->entries[i].importing;
        }
    }

    gmod_Module copy = module;
    size_t length = strlen(copy.name);
    if (length == SIZE_MAX) {
        return false;
    }

    char* name = galloc_alloc(alloc, length + 1);
    if (name == NULL) {
        return false;
    }

    memcpy(name, copy.name, length + 1);
    copy.name = name;
    if (!gmod__config_copy(alloc, &copy.config, &module.config)) {
        galloc_free(alloc, name);
        return false;
    }

    if (modules->count == modules->capacity) {
        size_t capacity = modules->capacity != 0 ? modules->capacity * 2 : 8;
        gmod__ModuleEntry* entries = NULL;
        if (capacity >= modules->capacity && capacity <= SIZE_MAX / sizeof(*entries)) {
            entries = galloc_realloc(
                alloc,
                modules->entries,
                modules->capacity * sizeof(*entries),
                capacity * sizeof(*entries)
            );
        }

        if (entries == NULL) {
            galloc_free(alloc, name);
            gmod__config_free(alloc, &copy.config);
            return false;
        }

        modules->entries = entries;
        modules->capacity = capacity;
    }

    size_t index = modules->count;
    modules->count += 1;
    modules->entries[index] = (gmod__ModuleEntry) {.module = copy, .importing = true, .loaded = false};
    modules->imports += 1;
    bool success = copy.import == NULL || copy.import(modules, modules->world, copy.user);
    modules->imports -= 1;

    // Nested imports may reallocate entries; refer to them by index.
    gmod__ModuleEntry entry = modules->entries[index];
    size_t following = modules->count - index - 1;
    if (following > 0) {
        memmove(&modules->entries[index], &modules->entries[index + 1], following * sizeof(entry));
    }

    if (!success) {
        modules->count -= 1;
        galloc_free(alloc, (void*)entry.module.name);
        gmod__config_free(alloc, &entry.module.config);
        return false;
    }

    entry.importing = false;
    modules->entries[modules->count - 1] = entry;
    return true;
}

bool gmod_Modules_load(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc)) {
        return false;
    }

    bool load_allowed = modules->phase == GMOD_IDLE && modules->imports == 0;
    if (!load_allowed) {
        return false;
    }

    return gmod__load_range(modules, 0, GMOD_IDLE);
}

bool gmod_Modules_import_active(galloc_Allocator alloc, gmod_Modules* modules, gmod_Module module) {
    if (!gmod__allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return false;
    }

    modules->phase = GMOD_IDLE;
    bool ok = gmod_Modules_import(alloc, modules, module);
    modules->phase = GMOD_ACTIVE;
    return ok;
}

bool gmod_Modules_load_pending(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return false;
    }

    size_t first = 0;
    while (first < modules->count && modules->entries[first].loaded) {
        first += 1;
    }

    for (size_t i = first; i < modules->count; i++) {
        if (modules->entries[i].loaded) {
            return false;
        }
    }

    return gmod__load_range(modules, first, GMOD_ACTIVE);
}

bool gmod_Modules_can_unload(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc) || modules->imports > 0) {
        return false;
    }

    if (modules->phase == GMOD_IDLE) {
        return true;
    }

    if (modules->phase != GMOD_ACTIVE || !gmod__preflight_unload(modules)) {
        return false;
    }

    modules->phase = GMOD_ACTIVE;
    return true;
}

bool gmod_Modules_unload_except_many(
    galloc_Allocator alloc,
    gmod_Modules* modules,
    const char* const* names,
    size_t count
) {
    if (!gmod__allocator_matches(modules, alloc) || (count > 0 && names == NULL)) {
        return false;
    }

    if (modules->phase == GMOD_IDLE) {
        return true;
    }

    if (modules->phase != GMOD_ACTIVE) {
        return false;
    }

    if (!gmod__preflight_unload(modules)) {
        return false;
    }

    for (size_t i = modules->count; i > 0; i--) {
        gmod__ModuleEntry* entry = &modules->entries[i - 1];
        bool retained = false;
        for (size_t j = 0; j < count; j++) {
            if (names[j] != NULL && strcmp(entry->module.name, names[j]) == 0) {
                retained = true;
                break;
            }
        }

        if (entry->loaded && !retained) {
            if (entry->module.unload != NULL) {
                entry->module.unload(modules->world, entry->module.user);
            }

            entry->loaded = false;
        }
    }

    modules->phase = GMOD_ACTIVE;
    return true;
}

bool gmod_Modules_unload_except_two(
    galloc_Allocator alloc,
    gmod_Modules* modules,
    const char* name,
    const char* other
) {
    if (name == NULL) {
        return false;
    }

    const char* names[] = {name, other};
    return gmod_Modules_unload_except_many(alloc, modules, names, 2);
}

bool gmod_Modules_unload_except(galloc_Allocator alloc, gmod_Modules* modules, const char* name) {
    return gmod_Modules_unload_except_two(alloc, modules, name, NULL);
}

bool gmod_Modules_unload(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc)) {
        return false;
    }

    if (modules->phase == GMOD_IDLE) {
        return true;
    }

    if (modules->phase != GMOD_ACTIVE) {
        return false;
    }

    if (!gmod__preflight_unload(modules)) {
        return false;
    }

    gmod__unload_range(modules, 0, modules->count);
    modules->phase = GMOD_IDLE;
    return true;
}

bool gmod_Modules_shutdown(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc) || modules->imports > 0) {
        return false;
    }

    if (modules->phase == GMOD_IDLE) {
        return true;
    }

    if (modules->phase != GMOD_ACTIVE || !gmod__preflight_unload(modules)) {
        return false;
    }

    gecs_World_clear_entities(modules->world);
    gecs_World_freeze(modules->world);
    gmod__unload_range(modules, 0, modules->count);
    modules->phase = GMOD_IDLE;
    return true;
}

bool gmod_Modules_update(galloc_Allocator alloc, gmod_Modules* modules) {
    return gmod__dispatch(alloc, modules, false);
}

bool gmod_Modules_render(galloc_Allocator alloc, gmod_Modules* modules) {
    return gmod__dispatch(alloc, modules, true);
}

const char* gmod_Modules_error(const gmod_Modules* modules) {
    return modules != NULL ? modules->error : "invalid module manager";
}

void gmod_Modules_end_frame(gmod_Modules* modules) {
    if (modules != NULL && modules->phase == GMOD_ACTIVE) {
        gecs_World_reset_temp(modules->world);
    }
}

bool gmod_Modules_frame(galloc_Allocator alloc, gmod_Modules* modules) {
    if (!gmod__allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return false;
    }

    bool ok = gmod_Modules_update(alloc, modules) && gmod_Modules_render(alloc, modules);
    gmod_Modules_end_frame(modules);
    return ok;
}

static bool gmod__allocator_matches(const gmod_Modules* modules, galloc_Allocator alloc) {
    return modules != NULL && modules->alloc.data == alloc.data && modules->alloc.ftable == alloc.ftable;
}

static char* gmod__copy_string(galloc_Allocator alloc, const char* source) {
    if (source == NULL) {
        return NULL;
    }

    size_t length = strlen(source);
    if (length == SIZE_MAX) {
        return NULL;
    }

    char* copy = galloc_alloc(alloc, length + 1);
    if (copy != NULL) {
        memcpy(copy, source, length + 1);
    }

    return copy;
}

static void gmod__config_free(galloc_Allocator alloc, gmod_ModuleConfig* config) {
    gmod_ModuleConfigEntry* entries = (gmod_ModuleConfigEntry*)config->entries;
    for (size_t i = 0; i < config->count; i++) {
        galloc_free(alloc, (void*)entries[i].key);
        if (entries[i].kind == GMOD_STRING) {
            galloc_free(alloc, (void*)entries[i].value.string);
        }
    }

    galloc_free(alloc, entries);
    *config = (gmod_ModuleConfig) {0};
}

static bool gmod__config_copy(galloc_Allocator alloc, gmod_ModuleConfig* dest, const gmod_ModuleConfig* source) {
    *dest = (gmod_ModuleConfig) {0};
    if (source->count == 0) {
        return true;
    }

    bool entries_fit = source->count <= SIZE_MAX / sizeof(gmod_ModuleConfigEntry);
    if (source->entries == NULL || !entries_fit) {
        return false;
    }

    gmod_ModuleConfigEntry* entries = galloc_calloc(alloc, source->count, sizeof(*entries));
    if (entries == NULL) {
        return false;
    }

    dest->entries = entries;
    for (size_t i = 0; i < source->count; i++) {
        const gmod_ModuleConfigEntry* item = &source->entries[i];
        bool kind_valid = item->kind >= GMOD_NULL && item->kind <= GMOD_DOUBLE;
        bool missing_string = item->kind == GMOD_STRING && item->value.string == NULL;
        bool duplicate_key = gmod_ModuleConfig_get(dest, item->key) != NULL;
        if (item->key == NULL || !kind_valid || missing_string || duplicate_key) {
            goto fail;
        }

        entries[i] = *item;
        entries[i].key = gmod__copy_string(alloc, item->key);
        if (item->kind == GMOD_STRING) {
            entries[i].value.string = NULL;
        }

        dest->count += 1;
        if (entries[i].key == NULL) {
            goto fail;
        }

        if (item->kind == GMOD_STRING) {
            entries[i].value.string = gmod__copy_string(alloc, item->value.string);
            if (entries[i].value.string == NULL) {
                goto fail;
            }
        }
    }

    return true;
fail:
    gmod__config_free(alloc, dest);
    return false;
}

static void gmod__unload_range(gmod_Modules* modules, size_t first, size_t last) {
    while (last > first) {
        last -= 1;
        gmod__ModuleEntry* entry = &modules->entries[last];
        if (!entry->loaded) {
            continue;
        }

        if (entry->module.unload != NULL) {
            entry->module.unload(modules->world, entry->module.user);
        }

        entry->loaded = false;
    }
}

static bool gmod__load_range(gmod_Modules* modules, size_t first, gmod__ModulePhase failure_phase) {
    modules->error[0] = '\0';
    modules->phase = GMOD_LOADING;
    size_t i = first;
    for (; i < modules->count; i++) {
        gmod__ModuleEntry* entry = &modules->entries[i];
        if (!gmod__call(modules, &entry->module, entry->module.load, "load")) {
            break;
        }

        entry->loaded = true;
    }

    if (i == modules->count) {
        for (size_t j = first; j < modules->count; j++) {
            gmod_Module* module = &modules->entries[j].module;
            if (!gmod__call(modules, module, module->start, "start")) {
                goto fail;
            }
        }

        modules->phase = GMOD_ACTIVE;
        return true;
    }

fail:
    modules->phase = GMOD_UNLOADING;
    gmod__unload_range(modules, first, i);
    modules->phase = failure_phase;
    return false;
}

static bool gmod__preflight_unload(gmod_Modules* modules) {
    // Preflight every loaded service, including modules retained by unload_except:
    // an active dependency can make unloading any dependent unsafe.
    modules->phase = GMOD_UNLOADING;
    for (size_t i = 0; i < modules->count; i++) {
        gmod__ModuleEntry* entry = &modules->entries[i];
        if (!entry->loaded || entry->module.can_unload == NULL) {
            continue;
        }

        bool unload_allowed = entry->module.can_unload(modules->world, entry->module.user);
        if (!unload_allowed) {
            modules->phase = GMOD_ACTIVE;
            return false;
        }
    }

    return true;
}

static bool gmod__dispatch(galloc_Allocator alloc, gmod_Modules* modules, bool render) {
    if (!gmod__allocator_matches(modules, alloc) || modules->phase != GMOD_ACTIVE) {
        return false;
    }

    modules->error[0] = '\0';
    bool ok = true;
    modules->phase = GMOD_DISPATCH;
    for (int32_t stage = -1; stage <= 1; stage++) {
        for (size_t i = 0; i < modules->count; i++) {
            gmod__ModuleEntry* entry = &modules->entries[i];
            gmod_Module* module = &entry->module;
            int32_t order = render ? module->render_order : module->update_order;
            int32_t current = (order > 0) - (order < 0);
            gmod_ModuleRun hook = render ? module->render : module->update;
            bool stage_matches = current == stage;

            if (entry->loaded && hook != NULL && stage_matches) {
                ok = gmod__call(modules, module, hook, render ? "render" : "update");
                if (!ok) {
                    goto done;
                }
            }
        }
    }

done:
    modules->phase = GMOD_ACTIVE;
    return ok;
}

static bool gmod__call(gmod_Modules* modules, const gmod_Module* module, gmod_ModuleRun hook, const char* operation) {
    if (hook == NULL) {
        return true;
    }

    modules->error[0] = '\0';
    bool ok = hook(modules->world, module->user, modules->error, sizeof(modules->error));
    modules->error[sizeof(modules->error) - 1] = '\0';
    if (ok) {
        modules->error[0] = '\0';
    } else if (modules->error[0] == '\0') {
        snprintf(modules->error, sizeof(modules->error), "module '%s' %s failed", module->name, operation);
    }

    return ok;
}

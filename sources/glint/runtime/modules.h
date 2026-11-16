#pragma once

#include <glint/config.h>
#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmod_Modules gmod_Modules;

#if GLINT_PLATFORM_WINDOWS
#define GMOD_EXPORT __declspec(dllexport)
#elif GLINT_COMPILER_GNU || GLINT_COMPILER_CLANG
#define GMOD_EXPORT __attribute__((visibility("default")))
#else
#define GMOD_EXPORT
#endif

#define GMOD_ABI_VERSION 5u

/// Put this first in a designated initializer, including for static modules.
#define GMOD_INIT .abi_version = GMOD_ABI_VERSION, .struct_size = sizeof(gmod_Module)

/// Optional host policy shared by modules that use platform devices.
typedef struct gmod_Host {
    bool headless;
    bool allow_unavailable_devices;
} gmod_Host;

typedef bool (*gmod_ModuleImport)(gmod_Modules* modules, gecs_World* world, void* user);

/// Fallible lifecycle hook.
///
/// # Errors
///
/// Return false on failure; optionally write a NUL-terminated message to `error`.
/// The buffer is empty on entry, has `error_size` bytes, and is borrowed for the call.
typedef bool (*gmod_ModuleRun)(gecs_World* world, void* user, char* error, size_t error_size);

typedef bool (*gmod_ModuleCanUnload)(gecs_World* world, void* user);
typedef void (*gmod_ModuleHook)(gecs_World* world, void* user);

typedef enum gmod_ModuleValueKind {
    GMOD_NULL,
    GMOD_BOOL,
    GMOD_STRING,
    GMOD_INT,
    GMOD_DOUBLE,
} gmod_ModuleValueKind;

typedef struct gmod_ModuleConfigEntry {
    const char* key;
    gmod_ModuleValueKind kind;

    union {
        bool boolean;
        const char* string;
        int64_t integer;
        double real;
    } value;
} gmod_ModuleConfigEntry;

typedef struct gmod_ModuleConfig {
    const gmod_ModuleConfigEntry* entries;
    size_t count;
} gmod_ModuleConfig;

typedef struct gmod_Module {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;

    void* user;

    gmod_ModuleImport import;
    gmod_ModuleRun load;
    gmod_ModuleRun start;
    gmod_ModuleHook unload;
    gmod_ModuleRun update;
    gmod_ModuleRun render;

    /// Sign selects early/default/late stage; dependencies keep their order within it.
    int32_t update_order;
    int32_t render_order;
    gmod_ModuleConfig config;

    /// Side-effect-free veto: false leaves all modules loaded for a later retry.
    gmod_ModuleCanUnload can_unload;
} gmod_Module;

/// Entry point type for a shared module.
///
/// # Linking
///
/// Use the host's Glint runtime and ABI version. Keep the library loaded until
/// its registered callbacks and destructors can no longer run.
typedef const gmod_Module* (*gmod_DescriptorFn)(void);

/// Create a module manager using the world's allocator. Not thread-safe.
///
/// # Ownership
///
/// The world must outlive the manager. Use its allocator for manager operations.
gmod_Modules* gmod_Modules_new(gecs_World* world);

bool gmod_Modules_free(galloc_Allocator alloc, gmod_Modules* modules);

/// Import a module and its dependencies once per name.
///
/// # Ownership
///
/// Copies the descriptor, name and configuration; borrows `user` and callbacks.
/// The import hook must clean up its own work on failure.
///
/// # Errors
///
/// Rejects cycles and imports during lifecycle hooks. Successfully imported
/// dependencies remain available if a later import fails.
bool gmod_Modules_import(galloc_Allocator alloc, gmod_Modules* modules, gmod_Module module);

/// Look up a module's configuration; NULL if the module is absent.
///
/// # Ownership
///
/// Borrowed until the next import or manager destruction. Look it up again after
/// importing dependencies.
const gmod_ModuleConfig* gmod_Modules_config(const gmod_Modules* modules, const char* name);

/// NULL means the key is absent; GMOD_NULL is an explicit null value.
const gmod_ModuleConfigEntry* gmod_ModuleConfig_get(const gmod_ModuleConfig* config, const char* key);

/// Load dependencies before dependents; unload in reverse order.
///
/// # Errors
///
/// On failure, rolls back loaded modules and retains the diagnostic in
/// `gmod_Modules_error`. A failing load hook must clean up its own resources.
bool gmod_Modules_load(galloc_Allocator alloc, gmod_Modules* modules);

/// Check unload vetoes without unloading anything.
///
/// # Errors
///
/// Returns false during callbacks or when a module vetoes unloading.
bool gmod_Modules_can_unload(galloc_Allocator alloc, gmod_Modules* modules);

bool gmod_Modules_unload(galloc_Allocator alloc, gmod_Modules* modules);

/// Clear entities, freeze the world, then unload modules in reverse order.
/// Contexts remain available until world destruction; the world cannot be reused.
bool gmod_Modules_shutdown(galloc_Allocator alloc, gmod_Modules* modules);

/// Queue an import for `gmod_Modules_load_pending`. Call outside lifecycle hooks.
bool gmod_Modules_import_active(galloc_Allocator alloc, gmod_Modules* modules, gmod_Module module);

/// Load queued modules while keeping existing modules active.
///
/// # Errors
///
/// A failed load rolls back only the newly loaded modules.
bool gmod_Modules_load_pending(galloc_Allocator alloc, gmod_Modules* modules);

/// Unload everything except `name`. Its dependencies must remain loaded too.
bool gmod_Modules_unload_except(galloc_Allocator alloc, gmod_Modules* modules, const char* name);

/// Retained modules must have all their dependencies retained too.
bool gmod_Modules_unload_except_many(
    galloc_Allocator alloc,
    gmod_Modules* modules,
    const char* const* names,
    size_t count
);

bool gmod_Modules_unload_except_two(galloc_Allocator alloc, gmod_Modules* modules, const char* name, const char* other);

/// Run update hooks until one fails.
///
/// # Errors
///
/// Returns false on hook failure or invalid manager state. See `gmod_Modules_error`.
bool gmod_Modules_update(galloc_Allocator alloc, gmod_Modules* modules);

/// Run render hooks until one fails. Uses the same error contract as update.
bool gmod_Modules_render(galloc_Allocator alloc, gmod_Modules* modules);

/// Update, render, then reset temporary memory, including on failure.
/// A failed update skips rendering.
bool gmod_Modules_frame(galloc_Allocator alloc, gmod_Modules* modules);

/// Last load/update/render error; empty after success.
///
/// # Ownership
///
/// Borrowed until the next load, update or render, or manager destruction.
/// Invalid-state calls leave the previous message unchanged.
const char* gmod_Modules_error(const gmod_Modules* modules);

/// Reset frame temporary memory. Call even on failure when driving update/render separately.
void gmod_Modules_end_frame(gmod_Modules* modules);

#ifdef __cplusplus
}
#endif

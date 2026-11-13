#ifndef GMOD_MODULES_H_
#define GMOD_MODULES_H_

#include <glint/alloc.h>
#include <glint/ecs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmod_Modules gmod_Modules;

/* Export a module entry point from a shared library; the host supplies loading
 * and symbol lookup. Use extern "C" on the definition when compiling as C++. */
#if defined(_WIN32)
#define GMOD_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define GMOD_EXPORT __attribute__((visibility("default")))
#else
#define GMOD_EXPORT
#endif

#define GMOD_ABI_VERSION 1u
/* Put this first in a designated initializer, including for static modules. */
#define GMOD_INIT .abi_version = GMOD_ABI_VERSION, .struct_size = sizeof(gmod_Module)

/* Retrieve using GECS_CONTEXT(alloc, world, gmod_FrameAllocator). Allocations
 * made through this value expire at the end of the current frame. */
typedef struct gmod_FrameAllocator {
    galloc_Allocator alloc;
} gmod_FrameAllocator;

typedef bool (*gmod_ModuleImport)(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user);
typedef bool (*gmod_ModuleLoad)(galloc_Allocator alloc, gecs_World *world, void *user);
typedef void (*gmod_ModuleHook)(galloc_Allocator alloc, gecs_World *world, void *user);

typedef enum gmod_ModuleValueKind {
    GMOD_NULL,
    GMOD_BOOL,
    GMOD_STRING,
    GMOD_INT,
    GMOD_DOUBLE,
} gmod_ModuleValueKind;

typedef struct gmod_ModuleConfigEntry {
    const char *key;
    gmod_ModuleValueKind kind;
    union {
        bool boolean;
        const char *string;
        int64_t integer;
        double real;
    } value;
} gmod_ModuleConfigEntry;

typedef struct gmod_ModuleConfig {
    const gmod_ModuleConfigEntry *entries;
    size_t count;
} gmod_ModuleConfig;

typedef struct gmod_Module {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    void *user;
    gmod_ModuleImport import;
    gmod_ModuleLoad load;
    gmod_ModuleHook unload;
    gmod_ModuleHook update;
    gmod_ModuleHook render;
    gmod_ModuleConfig config;
} gmod_Module;

/* Optional convention for an exported symbol, e.g.:
 * GMOD_EXPORT const gmod_Module *gmod_descriptor(void);
 * The function returns a borrowed descriptor; the caller may copy it to supply
 * per-instance user/config before import. There is no built-in dynamic loader.
 * ABI v1 requires the same platform, C calling convention, pointer width, and
 * compatible definitions of galloc_Allocator and Glint's C API on both sides. It is
 * not a serialized or cross-architecture format. Future compatible versions
 * may append fields; a v1 manager ignores bytes after its known v1 structure.
 *
 * Import copies the descriptor, name, configuration entries and strings before
 * calling its hook. It borrows user and callback pointers: keep user, callback
 * code, and the DLL loaded until manager destruction AND until any callbacks,
 * components, systems, or other resources registered by the module are gone.
 * The manager never frees user. Modules own their resources and must clean up
 * partial failed imports/loads; unload is not called for a failed load. A module
 * can import dependencies in its import hook, with the same allocator. The
 * module must not retain a pointer to its input descriptor or transient config.
 * Callbacks must not let C++ exceptions cross this C boundary.
 *
 * DLL ECS calls must resolve to the host's compatible Glint C implementation;
 * the host must export/link those symbols, or both must link the same shared
 * runtime. A private engine copy is not a supported ABI boundary. Registration
 * is per world: use the same stable, preferably namespaced name and size, and
 * the identical destructor address (including NULL), on every registration.
 * IDs must not be reused across worlds. Matching name, size and destructor does
 * not verify field layout, alignment, packing, semantics or ownership: both
 * sides must share a compatible component definition. ECS retains destructor
 * addresses in registrations even after clear; if a DLL registered one, keep it
 * loaded until the world is destroyed. Also keep it loaded while any observer,
 * system, or other registered code or data can still be used. */
typedef const gmod_Module *(*gmod_DescriptorFn)(void);

/* All operations must use the exact allocator (data and function table) passed
 * to new; allocator state/table and world must outlive the manager. The manager
 * is not thread-safe; callers must serialize access. */
gmod_Modules *gmod_Modules_new(galloc_Allocator alloc, gecs_World *world);
void gmod_Modules_free(galloc_Allocator alloc, gmod_Modules *modules);
/* Imports are idempotent by name. Cycles and imports during lifecycle hooks
 * fail. Dependencies imported successfully before a failure remain imported;
 * an import callback is responsible for cleaning up its own partial work. */
bool gmod_Modules_import(galloc_Allocator alloc, gmod_Modules *modules, const gmod_Module *module);
/* Import hooks can look up their own configuration by module name. Keys and
 * strings are NUL-terminated C strings (embedded NUL bytes are not supported).
 * The manager copies them using the manager's allocator before invoking the
 * hook. The returned config pointer is valid until the next import or manager
 * destruction; look it up again after importing dependencies. Entry and string
 * pointers remain valid until manager destruction, except when their module's
 * import fails. Duplicate keys and invalid values are rejected. Re-importing
 * a name retains its original configuration. A missing key returns NULL, while
 * a present GMOD_NULL entry represents an explicit null value. */
const gmod_ModuleConfig *gmod_Modules_config(const gmod_Modules *modules, const char *name);
const gmod_ModuleConfigEntry *gmod_ModuleConfig_get(const gmod_ModuleConfig *config, const char *key);
/* Dependencies load first and unload last. If a load fails, already-loaded
 * modules are unloaded and the failing callback must clean up its own work. */
bool gmod_Modules_load(galloc_Allocator alloc, gmod_Modules *modules);
void gmod_Modules_unload(galloc_Allocator alloc, gmod_Modules *modules);
/* Explicit staged loading: active imports are queued, then load_pending loads
 * only the new suffix. On failure that suffix is rolled back while previously
 * loaded modules stay active. Only call from outside module callbacks. */
bool gmod_Modules_import_active(galloc_Allocator alloc, gmod_Modules *modules, const gmod_Module *module);
bool gmod_Modules_load_pending(galloc_Allocator alloc, gmod_Modules *modules);
/* For hosts with a known shutdown dependency: unload everything except name,
 * then unload the retained module after dependents have stopped. The caller is
 * responsible for ensuring that the retained module has no unloaded dependency. */
void gmod_Modules_unload_except(galloc_Allocator alloc, gmod_Modules *modules, const char *name);
/* Dispatch either the named module (only=true) or every other loaded module. */
void gmod_Modules_update_named(galloc_Allocator alloc, gmod_Modules *modules, const char *name, bool only);
void gmod_Modules_render_named(galloc_Allocator alloc, gmod_Modules *modules, const char *name, bool only);
void gmod_Modules_update(galloc_Allocator alloc, gmod_Modules *modules);
void gmod_Modules_render(galloc_Allocator alloc, gmod_Modules *modules);
/* Calls update and render, then resets frame scratch storage. Modules that
 * call update/render separately must call end_frame after both. */
void gmod_Modules_frame(galloc_Allocator alloc, gmod_Modules *modules);
void gmod_Modules_end_frame(gmod_Modules *modules);

#ifdef __cplusplus
}
#endif
#endif // GMOD_MODULES_H_

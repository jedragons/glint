#pragma once

#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>
#include <glint/modules/assets/cache.h>
#include <glint/modules/script/script.h>
#include <glint/vfs/vfs.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gscript_behavior_Behaviors gscript_behavior_Behaviors;
typedef struct gscript_Asset gscript_Asset;

/// Manager-owned ECS component; inspect only. `state_ref` refers to the entity's Lua table.
typedef struct gscript_behavior_Behavior {
    gscript_Asset* asset;
    lua_State* lua;
    int state_ref;
} gscript_behavior_Behavior;

/// Create the world's behavior manager and register the "lua" asset type.
///
/// # Ownership
///
/// One manager per world. World, runtime, VFS and cache must outlive it.
gscript_behavior_Behaviors* gscript_behavior_new(
    gscript_lua_Runtime* script,
    gecs_World* world,
    gvfs_Vfs* vfs,
    gasset_Cache* cache
);

/// Returns the virtual path of a live cached script asset.
const char* gscript_asset_path(const gscript_Asset* asset);

/// Attach the entity's script and run setup. Each entity has one behavior.
///
/// # Errors
///
/// Initial load failures leave an error on the Lua stack.
bool gscript_behavior_attach(gscript_behavior_Behaviors* behaviors, gecs_Entity entity, const char* path);

/// Run behavior updates. Failed reloads retain the previous script.
///
/// # Errors
///
/// Returns false on infrastructure failure, leaving an error on the Lua stack.
/// Callback errors are logged and disable the shared script until successful reload.
bool gscript_behavior_update(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors);

/// Run remaining destroy hooks and release the manager, even if a hook fails.
///
/// # Errors
///
/// Returns false without teardown while the cache is busy or teardown is recursive.
bool gscript_behavior_destroy(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors);

#ifdef __cplusplus
}
#endif

#pragma once

#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>
#include <glint/math/vec2.h>
#include <glint/modules/assets/cache.h>
#include <glint/vfs/vfs.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gdim2_Dim2 gdim2_Dim2;

typedef struct gdim2_Transform2D {
    gvec2_Vec2 position;
    gvec2_Vec2 scale;
    gvec2_Vec2 origin;
    float rotation;
} gdim2_Transform2D;

/// Create one 2D instance per world and cache.
///
/// # Ownership
///
/// World, VFS and borrowed cache must outlive the instance. Destroy it before closing
/// the graphics context. The world's allocator must outlive any Lua references.
gdim2_Dim2* gdim2_new(gecs_World* world, gvfs_Vfs* vfs, gasset_Cache* assets);

/// Returns false without teardown while its cache is busy; retry after the callback or poll returns.
/// NULL succeeds.
bool gdim2_destroy(galloc_Allocator alloc, gdim2_Dim2* dim2);

/// Whether an active cache callback or poll prevents destruction. NULL is not busy.
bool gdim2_busy(const gdim2_Dim2* dim2);

/// Advance animation; polls only a privately owned cache. The host polls shared caches.
void gdim2_update(galloc_Allocator alloc, gdim2_Dim2* dim2, float delta);

/// Draw 2D entities inside an active drawing frame.
void gdim2_render(gdim2_Dim2* dim2);

#ifdef __cplusplus
}
#endif

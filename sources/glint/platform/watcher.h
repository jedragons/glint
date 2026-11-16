#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Poll individual asset files from the update thread. The same allocator must
// be supplied for all operations on a watcher and must outlive the watcher.
typedef struct gwatch_Watcher gwatch_Watcher;
typedef void (*gwatch_ChangedFn)(const char* path, void* user);

gwatch_Watcher* gwatch_create(galloc_Allocator alloc);

void gwatch_destroy(galloc_Allocator alloc, gwatch_Watcher* watcher);

/// Watch a UTF-8 path; duplicate paths are ignored.
///
/// # Errors
///
/// Returns false on failure.
bool gwatch_add(galloc_Allocator alloc, gwatch_Watcher* watcher, const char* path);

/// Report modification, deletion and reappearance on the polling thread.
/// Changes between polls may be missed.
///
/// # Ownership
///
/// Callback paths live until watcher destruction. Do not destroy it inside a callback.
void gwatch_poll(gwatch_Watcher* watcher, gwatch_ChangedFn changed, void* user);

#ifdef __cplusplus
}
#endif

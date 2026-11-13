#ifndef GWATCH_CORE_ASSETS_FILE_WATCHER_H_
#define GWATCH_CORE_ASSETS_FILE_WATCHER_H_

#include <glint/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Poll individual asset files from the update thread. The same allocator must
 * be supplied for all operations on a watcher and must outlive the watcher. */
typedef struct gwatch_Watcher gwatch_Watcher;
typedef void (*gwatch_ChangedFn)(const char *path, void *user);

gwatch_Watcher *gwatch_create(galloc_Allocator alloc);
void gwatch_destroy(galloc_Allocator alloc, gwatch_Watcher *watcher);
/* Paths are UTF-8. Duplicate paths are ignored; returns 0 on failure. */
int gwatch_add(galloc_Allocator alloc, gwatch_Watcher *watcher, const char *path);
/* Reports modification, deletion and reappearance. Paths remain valid until
 * the watcher is destroyed. Do not destroy the watcher inside the callback.
 * Multiple changes between polls may be coalesced or missed. */
void gwatch_poll(gwatch_Watcher *watcher, gwatch_ChangedFn changed, void *user);

#ifdef __cplusplus
}
#endif

#endif // GWATCH_CORE_ASSETS_FILE_WATCHER_H_

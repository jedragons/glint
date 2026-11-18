#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gvfs_Vfs gvfs_Vfs;

/// Return false to stop listing directory entry names.
typedef bool (*gvfs_EntryFn)(const char* name, void* user);

/// Backend paths are relative to the mount root. `list` visits names and returns
/// true on successful enumeration, including an early visitor stop.
///
/// # Ownership
///
/// `read` and `real_path` allocate with the supplied allocator. The VFS owns
/// `context` after a successful mount and calls `destroy` at teardown.
/// `real_path` may be NULL for virtual backends.
typedef struct gvfs_Filesystem {
    void* context;
    bool (*exists)(galloc_Allocator alloc, void* context, const char* path);
    void* (*read)(galloc_Allocator alloc, void* context, const char* path, size_t* size);
    bool (*list)(galloc_Allocator alloc, void* context, const char* path, gvfs_EntryFn visit, void* user);
    char* (*real_path)(galloc_Allocator alloc, void* context, const char* path);
    void (*destroy)(galloc_Allocator alloc, void* context);
} gvfs_Filesystem;

/// Get the native UTF-8 working directory; NULL on failure.
///
/// # Ownership
///
/// Free the returned path with `alloc`.
char* gvfs_get_working_directory(galloc_Allocator alloc);

/// Resolve virtual absolute paths by the most specific mount, then mount order.
gvfs_Vfs* gvfs_create(galloc_Allocator alloc);

void gvfs_destroy(galloc_Allocator alloc, gvfs_Vfs* vfs);

bool gvfs_mount(galloc_Allocator alloc, gvfs_Vfs* vfs, const char* point, const char* directory);

/// Mount a filesystem.
///
/// # Ownership
///
/// Transfers the backend context on success; the caller retains it on failure.
bool gvfs_mount_filesystem(galloc_Allocator alloc, gvfs_Vfs* vfs, const char* point, gvfs_Filesystem filesystem);

/// Read a file; NULL on failure.
///
/// # Ownership
///
/// Free the result with `alloc`, including an empty file.
void* gvfs_read(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path, size_t* size);

/// Like `gvfs_read`, with a trailing NUL.
char* gvfs_read_str(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path);

bool gvfs_exists(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path);

/// Return false to stop enumeration. Point strings remain valid until the next mount.
typedef bool (*gvfs_PointsFn)(const char* point, void* user);

/// Visit each mount point once, most specific first. False on failure or no mounts.
bool gvfs_points(const gvfs_Vfs* vfs, gvfs_PointsFn visit, void* user);

/// Return false to stop enumeration. Path strings are borrowed for the call.
typedef bool (*gvfs_ListFn)(const char* path, void* user);

/// Visit entries across matching mounts. False on failure or no matching directories.
bool gvfs_list(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path, gvfs_ListFn visit, void* user);

/// Find a native path; NULL if unavailable.
///
/// # Ownership
///
/// Free the result with `alloc`.
char* gvfs_real_path(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path);

#ifdef __cplusplus
}
#endif

#ifndef GVFS_C_VFS_H
#define GVFS_C_VFS_H

#include <stdbool.h>
#include <stddef.h>
#include <glint/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gvfs_Vfs gvfs_Vfs;
/* Return the process working directory as a native UTF-8 path, allocated with
 * alloc. Returns NULL on OS, encoding, or allocation failure. */
char *gvfs_get_working_directory(galloc_Allocator alloc);
/* Paths are virtual absolute paths. Mounts are searched newest first. */
gvfs_Vfs *gvfs_create(galloc_Allocator alloc);
void gvfs_destroy(galloc_Allocator alloc, gvfs_Vfs *vfs);
int gvfs_mount(galloc_Allocator alloc, gvfs_Vfs *vfs, const char *point, const char *directory);
/* Caller frees the returned buffer with alloc, including for an empty file. */
void *gvfs_read(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path, size_t *size);
/* Read a file and NUL-terminate it; NULL when the file is missing or a read
 * fails. The caller frees the result with alloc, including an empty file. */
char *gvfs_read_str(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path);
/* Returns 1 when an existing file resolves under a mount. */
bool gvfs_exists(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path);
/* Enumerate the mounted points, newest mount first, each point once. Points
 * passed to the callback stay valid until the next mount; returning 0 stops
 * enumeration. Returns 0 on failure and 1 when at least one point was
 * enumerated. */
typedef int (*gvfs_PointsFn)(const char *point, void *user);
int gvfs_points(const gvfs_Vfs *vfs, gvfs_PointsFn visit, void *user);
/* Enumerate matching directories across mounts. Paths passed to the callback
 * are temporary; returning 0 stops enumeration. Returns 0 on allocation or
 * enumeration failure, and 1 when at least one directory was enumerated. */
typedef int (*gvfs_ListFn)(const char *path, void *user);
int gvfs_list(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path, gvfs_ListFn visit, void *user);
/* Returns a native path allocated with alloc if an existing file is found. */
char *gvfs_real_path(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path);

#ifdef __cplusplus
}
#endif
#endif

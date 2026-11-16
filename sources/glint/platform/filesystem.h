#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Result of a native filesystem operation.
typedef enum gfs_Result {
    /// The operation completed successfully.
    GFS_OK,

    /// The file or directory could not be opened (for example, missing or inaccessible).
    GFS_UNAVAILABLE,

    /// Invalid arguments, invalid path encoding, size overflow, or an I/O or enumeration failure.
    GFS_ERROR,

    /// Memory allocation failed.
    GFS_OOM,
} gfs_Result;

/// Return false to stop listing. Names are borrowed for the callback.
typedef bool (*gfs_EntryFn)(const char* name, void* user);

/// Return the UTF-8 working directory, allocated with `alloc`; NULL on failure.
char* gfs_working_directory(galloc_Allocator alloc);

bool gfs_exists(galloc_Allocator alloc, const char* path);

/// Read a native file, allocating even for an empty file. Free the result with `alloc`.
/// `out` is required; `size` may be NULL. Outputs are cleared on failure.
///
/// ## Errors
///
/// - `GFS_UNAVAILABLE`
/// - `GFS_ERROR`
/// - `GFS_OOM`
gfs_Result gfs_read(galloc_Allocator alloc, const char* path, void** out, size_t* size);

/// List entry names, excluding `.` and `..`. An early visitor stop succeeds.
///
/// ## Errors
///
/// - `GFS_UNAVAILABLE`
/// - `GFS_ERROR`
/// - `GFS_OOM`
gfs_Result gfs_list(galloc_Allocator alloc, const char* path, gfs_EntryFn visit, void* user);

#ifdef __cplusplus
}
#endif

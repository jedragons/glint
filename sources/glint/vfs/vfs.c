#include <glint/platform/filesystem.h>
#include <glint/vfs/vfs.h>

#include <stdint.h>
#include <string.h>

typedef struct gvfs__Mount {
    char* point;
    char* directory;
    gvfs_Filesystem filesystem;
    bool custom;
    struct gvfs__Mount* next;
} gvfs__Mount;

struct gvfs_Vfs {
    gvfs__Mount* mounts;
};

typedef struct gvfs__ListAdapter {
    galloc_Allocator alloc;
    const char* path;
    gvfs_ListFn visit;
    void* user;
    int32_t result;
} gvfs__ListAdapter;

static char* gvfs__copy_string(galloc_Allocator alloc, const char* s, size_t length);
static bool gvfs__valid_path(const char* path);
static void gvfs__insert_mount(gvfs_Vfs* vfs, gvfs__Mount* mount);
static const char* gvfs__relative(const gvfs__Mount* mount, const char* path);
static char* gvfs__candidate(galloc_Allocator alloc, const gvfs__Mount* mount, const char* path);
static int32_t gvfs__visit_entry(
    galloc_Allocator alloc,
    const char* directory,
    const char* name,
    gvfs_ListFn visit,
    void* user
);
static bool gvfs__visit_native_entry(const char* name, void* user);
static bool gvfs__visit_backend_entry(const char* name, void* user);

char* gvfs_get_working_directory(galloc_Allocator alloc) {
    return gfs_working_directory(alloc);
}

gvfs_Vfs* gvfs_create(galloc_Allocator alloc) {
    return galloc_calloc(alloc, 1, sizeof(gvfs_Vfs));
}

void gvfs_destroy(galloc_Allocator alloc, gvfs_Vfs* vfs) {
    if (vfs == NULL) {
        return;
    }

    while (vfs->mounts != NULL) {
        gvfs__Mount* mount = vfs->mounts;
        vfs->mounts = mount->next;
        galloc_free(alloc, mount->point);
        galloc_free(alloc, mount->directory);
        if (mount->custom && mount->filesystem.destroy != NULL) {
            mount->filesystem.destroy(alloc, mount->filesystem.context);
        }

        galloc_free(alloc, mount);
    }

    galloc_free(alloc, vfs);
}

bool gvfs_mount_filesystem(galloc_Allocator alloc, gvfs_Vfs* vfs, const char* point, gvfs_Filesystem filesystem) {
    if (vfs == NULL || !gvfs__valid_path(point)) {
        return false;
    }

    bool backend_complete = filesystem.exists != NULL && filesystem.read != NULL && filesystem.list != NULL;
    if (!backend_complete) {
        return false;
    }

    gvfs__Mount* mount = galloc_calloc(alloc, 1, sizeof(*mount));
    if (mount == NULL) {
        return false;
    }

    size_t point_length = strlen(point);
    while (point_length > 1 && point[point_length - 1] == '/') {
        point_length -= 1;
    }

    mount->point = gvfs__copy_string(alloc, point, point_length);
    if (mount->point == NULL) {
        galloc_free(alloc, mount);
        return false;
    }

    mount->custom = true;
    mount->filesystem = filesystem;
    gvfs__insert_mount(vfs, mount);
    return true;
}

bool gvfs_mount(galloc_Allocator alloc, gvfs_Vfs* vfs, const char* point, const char* directory) {
    if (vfs == NULL || !gvfs__valid_path(point) || directory == NULL || *directory == 0) {
        return false;
    }

    gvfs__Mount* mount = galloc_calloc(alloc, 1, sizeof(*mount));
    if (mount == NULL) {
        return false;
    }

    size_t point_length = strlen(point);
    while (point_length > 1 && point[point_length - 1] == '/') {
        point_length -= 1;
    }

    mount->point = gvfs__copy_string(alloc, point, point_length);
    mount->directory = gvfs__copy_string(alloc, directory, strlen(directory));
    if (mount->point == NULL || mount->directory == NULL) {
        galloc_free(alloc, mount->point);
        galloc_free(alloc, mount->directory);
        galloc_free(alloc, mount);
        return false;
    }

    gvfs__insert_mount(vfs, mount);
    return true;
}

char* gvfs_real_path(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path) {
    if (vfs == NULL || !gvfs__valid_path(path)) {
        return NULL;
    }

    for (const gvfs__Mount* mount = vfs->mounts; mount != NULL; mount = mount->next) {
        if (mount->custom) {
            const char* relative = gvfs__relative(mount, path);
            if (relative == NULL || !mount->filesystem.exists(alloc, mount->filesystem.context, relative)) {
                continue;
            }
            return mount->filesystem.real_path != NULL
                ? mount->filesystem.real_path(alloc, mount->filesystem.context, relative)
                : NULL;
        }

        char* name = gvfs__candidate(alloc, mount, path);
        if (name == NULL) {
            continue;
        }

        if (gfs_exists(alloc, name)) {
            return name;
        }

        galloc_free(alloc, name);
    }

    return NULL;
}

void* gvfs_read(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path, size_t* size) {
    if (size != NULL) {
        *size = 0;
    }

    if (vfs == NULL || !gvfs__valid_path(path)) {
        return NULL;
    }

    for (const gvfs__Mount* mount = vfs->mounts; mount != NULL; mount = mount->next) {
        if (mount->custom) {
            const char* relative = gvfs__relative(mount, path);
            if (relative != NULL && mount->filesystem.exists(alloc, mount->filesystem.context, relative)) {
                return mount->filesystem.read(alloc, mount->filesystem.context, relative, size);
            }
        } else {
            char* name = gvfs__candidate(alloc, mount, path);
            if (name == NULL) {
                continue;
            }

            void* data = NULL;
            gfs_Result result = gfs_read(alloc, name, &data, size);
            galloc_free(alloc, name);
            if (result != GFS_UNAVAILABLE) {
                return data;
            }
        }
    }

    return NULL;
}

char* gvfs_read_str(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path) {
    size_t size = 0;
    void* data = gvfs_read(alloc, vfs, path, &size);
    if (data == NULL) {
        return NULL;
    }

    if (size == SIZE_MAX) {
        galloc_free(alloc, data);
        return NULL;
    }

    char* result = galloc_realloc(alloc, data, size != 0 ? size : 1, size + 1);
    if (result == NULL) {
        galloc_free(alloc, data);
        return NULL;
    }

    result[size] = '\0';
    return result;
}

bool gvfs_exists(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path) {
    if (vfs == NULL || !gvfs__valid_path(path)) {
        return false;
    }

    for (const gvfs__Mount* mount = vfs->mounts; mount != NULL; mount = mount->next) {
        const char* relative = gvfs__relative(mount, path);
        if (relative == NULL) {
            continue;
        }

        if (mount->custom) {
            if (mount->filesystem.exists(alloc, mount->filesystem.context, relative)) {
                return true;
            }
        } else {
            char* name = gvfs__candidate(alloc, mount, path);
            if (name == NULL) {
                continue;
            }

            bool exists = gfs_exists(alloc, name);
            galloc_free(alloc, name);
            if (exists) {
                return true;
            }
        }
    }

    return false;
}

bool gvfs_points(const gvfs_Vfs* vfs, gvfs_PointsFn visit, void* user) {
    if (vfs == NULL || visit == NULL) {
        return false;
    }

    bool found = false;
    // Repeated mount points are returned once.
    for (const gvfs__Mount* mount = vfs->mounts; mount != NULL; mount = mount->next) {
        bool duplicate = false;
        for (const gvfs__Mount* previous = vfs->mounts; previous != mount; previous = previous->next) {
            if (strcmp(previous->point, mount->point) == 0) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            continue;
        }

        found = true;
        if (!visit(mount->point, user)) {
            return true;
        }
    }

    return found;
}

bool gvfs_list(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path, gvfs_ListFn visit, void* user) {
    if (vfs == NULL || !gvfs__valid_path(path) || visit == NULL) {
        return false;
    }

    bool found = false;
    for (const gvfs__Mount* mount = vfs->mounts; mount != NULL; mount = mount->next) {
        gvfs__ListAdapter adapter = {.alloc = alloc, .path = path, .visit = visit, .user = user, .result = 1};
        bool listed;
        if (mount->custom) {
            const char* relative = gvfs__relative(mount, path);
            if (relative == NULL) {
                continue;
            }

            listed =
                mount->filesystem.list(alloc, mount->filesystem.context, relative, gvfs__visit_backend_entry, &adapter);
        } else {
            char* directory = gvfs__candidate(alloc, mount, path);
            if (directory == NULL) {
                continue;
            }

            gfs_Result result = gfs_list(alloc, directory, gvfs__visit_native_entry, &adapter);
            galloc_free(alloc, directory);
            if (result != GFS_OK && result != GFS_UNAVAILABLE) {
                return false;
            }

            listed = result == GFS_OK;
        }

        if (adapter.result <= 0) {
            return adapter.result == 0;
        }

        if (listed) {
            found = true;
        }
    }

    return found;
}

static char* gvfs__copy_string(galloc_Allocator alloc, const char* s, size_t length) {
    char* out = galloc_alloc(alloc, length + 1);
    if (out != NULL) {
        memcpy(out, s, length);
        out[length] = '\0';
    }

    return out;
}

// Reject ambiguous paths and traversal; virtual paths always use '/'.
static bool gvfs__valid_path(const char* path) {
    if (path == NULL || *path != '/') {
        return false;
    }

    for (const char* p = path + 1; *p != 0;) {
        const char* start = p;
        while (*p != 0 && *p != '/') {
            if (*p == '\\' || *p == ':') {
                return false;
            }

            p += 1;
        }

        size_t segment_length = (size_t)(p - start);
        bool current_directory = segment_length == 1 && start[0] == '.';
        bool parent_directory = segment_length == 2 && start[0] == '.' && start[1] == '.';
        if (segment_length == 0 || current_directory || parent_directory) {
            return false;
        }

        if (*p != 0) {
            p += 1;
        }
    }

    return true;
}

static void gvfs__insert_mount(gvfs_Vfs* vfs, gvfs__Mount* mount) {
    gvfs__Mount** next = &vfs->mounts;
    size_t length = strlen(mount->point);
    while (*next != NULL && strlen((*next)->point) >= length) {
        next = &(*next)->next;
    }

    mount->next = *next;
    *next = mount;
}

static const char* gvfs__relative(const gvfs__Mount* mount, const char* path) {
    size_t mount_length = strlen(mount->point);
    if (mount_length == 1) {
        return path + 1;
    }

    if (strncmp(path, mount->point, mount_length) != 0) {
        return NULL;
    }

    bool mount_boundary = path[mount_length] == '\0' || path[mount_length] == '/';
    if (!mount_boundary) {
        return NULL;
    }

    return path + mount_length + (path[mount_length] == '/');
}

static char* gvfs__candidate(galloc_Allocator alloc, const gvfs__Mount* mount, const char* path) {
    size_t mount_length = strlen(mount->point);
    size_t directory_length = strlen(mount->directory);
    const char* relative;
    if (mount_length == 1) {
        relative = path + 1;
    } else {
        if (strncmp(path, mount->point, mount_length) != 0) {
            return NULL;
        }

        bool mount_boundary = path[mount_length] == '\0' || path[mount_length] == '/';
        if (!mount_boundary) {
            return NULL;
        }

        relative = path + mount_length;
        if (*relative == '/') {
            relative += 1;
        }
    }

    size_t relative_length = strlen(relative);
    if (directory_length > SIZE_MAX - relative_length - 2) {
        return NULL;
    }

    char* result = galloc_alloc(alloc, directory_length + relative_length + 2);
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, mount->directory, directory_length);
    result[directory_length] = '/';
    memcpy(result + directory_length + 1, relative, relative_length + 1);
    return result;
}

static int32_t gvfs__visit_entry(
    galloc_Allocator alloc,
    const char* directory,
    const char* name,
    gvfs_ListFn visit,
    void* user
) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 1;
    }

    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    bool slash = directory_length > 0 && directory[directory_length - 1] != '/';
    if (directory_length > SIZE_MAX - name_length - 2) {
        return -1;
    }

    char* path = galloc_alloc(alloc, directory_length + (size_t)slash + name_length + 1);
    if (path == NULL) {
        return -1;
    }

    memcpy(path, directory, directory_length);
    if (slash) {
        path[directory_length] = '/';
        directory_length += 1;
    }

    memcpy(path + directory_length, name, name_length + 1);
    bool result = visit(path, user);
    galloc_free(alloc, path);
    return result ? 1 : 0;
}

static bool gvfs__visit_native_entry(const char* name, void* user) {
    gvfs__ListAdapter* adapter = user;
    adapter->result = gvfs__visit_entry(adapter->alloc, adapter->path, name, adapter->visit, adapter->user);
    return adapter->result > 0;
}

static bool gvfs__visit_backend_entry(const char* name, void* user) {
    gvfs__ListAdapter* adapter = user;
    if (name == NULL || strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
        adapter->result = -1;
        return false;
    }

    return gvfs__visit_native_entry(name, user);
}

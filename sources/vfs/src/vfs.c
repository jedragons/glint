#include <glint/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    /* Avoid collisions with raylib names in unity builds. */
    #define Rectangle gvfs__Win32Rectangle
    #define CloseWindow gvfs__Win32CloseWindow
    #define ShowCursor gvfs__Win32ShowCursor
    #include <windows.h>
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
#else
    #include <dirent.h>
    #include <errno.h>
    #include <unistd.h>
#endif
#include <stdint.h>

typedef struct gvfs__Mount {
    char *point, *directory;
    struct gvfs__Mount *next;
} gvfs__Mount;

struct gvfs_Vfs {
    gvfs__Mount *mounts;
};

char *gvfs_get_working_directory(galloc_Allocator alloc) {
#ifdef _WIN32
    DWORD capacity = GetCurrentDirectoryW(0, NULL);
    while (capacity) {
        if ((size_t)capacity > SIZE_MAX / sizeof(wchar_t)) {
            return NULL;
        }
        wchar_t *wide = galloc_alloc(alloc, (size_t)capacity * sizeof(*wide));
        if (!wide) {
            return NULL;
        }
        DWORD length = GetCurrentDirectoryW(capacity, wide);
        if (!length) {
            galloc_free(alloc, wide);
            return NULL;
        }
        if (length >= capacity) {
            galloc_free(alloc, wide);
            capacity = length;
            continue;
        }
        int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
        char *path = bytes > 0 ? galloc_alloc(alloc, (size_t)bytes) : NULL;
        if (path && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, path, bytes, NULL, NULL)) {
            galloc_free(alloc, path);
            path = NULL;
        }
        galloc_free(alloc, wide);
        return path;
    }
    return NULL;
#else
    size_t capacity = 256;
    for (;;) {
        char *path = galloc_alloc(alloc, capacity);
        if (!path) {
            return NULL;
        }
        if (getcwd(path, capacity)) {
            return path;
        }
        int error = errno;
        galloc_free(alloc, path);
        if (error != ERANGE || capacity > SIZE_MAX / 2) {
            return NULL;
        }
        capacity *= 2;
    }
#endif
}

static char *gvfs__vfs_vfs_copy_string(galloc_Allocator alloc, const char *s, size_t length) {
    char *out = galloc_alloc(alloc, length + 1);
    if (out) {
        memcpy(out, s, length);
        out[length] = '\0';
    }
    return out;
}

/* Reject ambiguous paths and traversal; virtual paths always use '/'. */
static int gvfs__vfs_valid_path(const char *path) {
    const char *p;
    if (!path || *path != '/')
        return 0;
    for (p = path + 1; *p;) {
        const char *start = p;
        while (*p && *p != '/') {
            if (*p == '\\' || *p == ':')
                return 0;
            ++p;
        }
        if (p == start || (p - start == 1 && start[0] == '.') || (p - start == 2 && start[0] == '.' && start[1] == '.'))
            return 0;
        if (*p)
            ++p;
    }
    return 1;
}

gvfs_Vfs *gvfs_create(galloc_Allocator alloc) {
    return galloc_calloc(alloc, 1, sizeof(gvfs_Vfs));
}

void gvfs_destroy(galloc_Allocator alloc, gvfs_Vfs *vfs) {
    if (!vfs)
        return;
    while (vfs->mounts) {
        gvfs__Mount *m = vfs->mounts;
        vfs->mounts = m->next;
        galloc_free(alloc, m->point);
        galloc_free(alloc, m->directory);
        galloc_free(alloc, m);
    }
    galloc_free(alloc, vfs);
}

int gvfs_mount(galloc_Allocator alloc, gvfs_Vfs *vfs, const char *point, const char *directory) {
    gvfs__Mount *m;
    size_t n;
    if (!vfs || !gvfs__vfs_valid_path(point) || !directory || !*directory)
        return 0;
    m = galloc_calloc(alloc, 1, sizeof(*m));
    if (!m)
        return 0;
    n = strlen(point);
    while (n > 1 && point[n - 1] == '/')
        --n;
    m->point = gvfs__vfs_vfs_copy_string(alloc, point, n);
    m->directory = gvfs__vfs_vfs_copy_string(alloc, directory, strlen(directory));
    if (!m->point || !m->directory) {
        galloc_free(alloc, m->point);
        galloc_free(alloc, m->directory);
        galloc_free(alloc, m);
        return 0;
    }
    m->next = vfs->mounts;
    vfs->mounts = m;
    return 1;
}

static char *gvfs__vfs_candidate(galloc_Allocator alloc, const gvfs__Mount *m, const char *path) {
    size_t plen = strlen(m->point), dlen = strlen(m->directory), rlen;
    const char *rest;
    char *result;
    if (plen == 1)
        rest = path + 1;
    else {
        if (strncmp(path, m->point, plen) || (path[plen] && path[plen] != '/'))
            return NULL;
        rest = path + plen;
        if (*rest == '/')
            ++rest;
    }
    rlen = strlen(rest);
    if (dlen > SIZE_MAX - rlen - 2)
        return NULL;
    result = galloc_alloc(alloc, dlen + rlen + 2);
    if (!result)
        return NULL;
    memcpy(result, m->directory, dlen);
    result[dlen] = '/';
    memcpy(result + dlen + 1, rest, rlen + 1);
    return result;
}

static FILE *gvfs__vfs_open_file(galloc_Allocator alloc, const char *path) {
#ifdef _WIN32
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (length <= 0 || (size_t)length > SIZE_MAX / sizeof(wchar_t)) {
        return NULL;
    }
    wchar_t *wide = galloc_alloc(alloc, (size_t)length * sizeof(*wide));
    if (wide == NULL) {
        return NULL;
    }
    FILE *file = NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, length)) {
        file = _wfopen(wide, L"rb");
    }
    galloc_free(alloc, wide);
    return file;
#else
    (void)alloc;
    return fopen(path, "rb");
#endif
}

char *gvfs_real_path(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path) {
    const gvfs__Mount *m;
    if (!vfs || !gvfs__vfs_valid_path(path))
        return NULL;
    for (m = vfs->mounts; m; m = m->next) {
        char *name = gvfs__vfs_candidate(alloc, m, path);
        FILE *f;
        if (!name)
            continue;
        f = gvfs__vfs_open_file(alloc, name);
        if (f) {
            fclose(f);
            return name;
        }
        galloc_free(alloc, name);
    }
    return NULL;
}

void *gvfs_read(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path, size_t *size) {
    char *name = gvfs_real_path(alloc, vfs, path);
    FILE *f;
    long length;
    void *data;
    if (size)
        *size = 0;
    if (!name)
        return NULL;
    f = gvfs__vfs_open_file(alloc, name);
    galloc_free(alloc, name);
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) || (length = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return NULL;
    }
    data = galloc_alloc(alloc, length ? (size_t)length : 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    if (fread(data, 1, (size_t)length, f) != (size_t)length) {
        galloc_free(alloc, data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if (size)
        *size = (size_t)length;
    return data;
}

char *gvfs_read_str(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path) {
    size_t size = 0;
    void *data = gvfs_read(alloc, vfs, path, &size);
    if (!data) {
        return NULL;
    }
    if (size == SIZE_MAX) {
        galloc_free(alloc, data);
        return NULL;
    }
    char *result = galloc_realloc(alloc, data, size ? size : 1, size + 1);
    if (!result) {
        galloc_free(alloc, data);
        return NULL;
    }
    result[size] = '\0';
    return result;
}

bool gvfs_exists(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path) {
    char *name = gvfs_real_path(alloc, vfs, path);
    if (!name) {
        return false;
    }
    galloc_free(alloc, name);
    return true;
}

int gvfs_points(const gvfs_Vfs *vfs, gvfs_PointsFn visit, void *user) {
    if (!vfs || !visit) {
        return 0;
    }
    int found = 0;
    /* The mount list is newest first, so the first occurrence of a point is
     * its newest mount. */
    for (const gvfs__Mount *m = vfs->mounts; m; m = m->next) {
        int duplicate = 0;
        for (const gvfs__Mount *previous = vfs->mounts; previous != m; previous = previous->next) {
            if (strcmp(previous->point, m->point) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        found = 1;
        if (!visit(m->point, user)) {
            return 1;
        }
    }
    return found;
}

static int
gvfs__vfs_vfs_visit_entry(galloc_Allocator alloc, const char *directory, const char *name, gvfs_ListFn visit, void *user) {
    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        return 1;
    }
    size_t dlen = strlen(directory), nlen = strlen(name);
    int slash = dlen > 0 && directory[dlen - 1] != '/';
    if (dlen > SIZE_MAX - nlen - 2) {
        return -1;
    }
    char *path = galloc_alloc(alloc, dlen + (size_t)slash + nlen + 1);
    if (!path) {
        return -1;
    }
    memcpy(path, directory, dlen);
    if (slash) {
        path[dlen++] = '/';
    }
    memcpy(path + dlen, name, nlen + 1);
    int result = visit(path, user);
    galloc_free(alloc, path);
    return result ? 1 : 0;
}

int gvfs_list(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path, gvfs_ListFn visit, void *user) {
    if (!vfs || !gvfs__vfs_valid_path(path) || !visit) {
        return 0;
    }
    int found = 0;
    for (const gvfs__Mount *m = vfs->mounts; m; m = m->next) {
        char *directory = gvfs__vfs_candidate(alloc, m, path);
        if (!directory) {
            continue;
        }
#ifdef _WIN32
        size_t bytes = strlen(directory);
        if (bytes > SIZE_MAX - 3) {
            galloc_free(alloc, directory);
            return 0;
        }
        char *pattern = galloc_alloc(alloc, bytes + 3);
        if (!pattern) {
            galloc_free(alloc, directory);
            return 0;
        }
        memcpy(pattern, directory, bytes);
        memcpy(pattern + bytes, "/*", 3);
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pattern, -1, NULL, 0);
        if (length <= 0 || (size_t)length > SIZE_MAX / sizeof(wchar_t)) {
            galloc_free(alloc, pattern);
            galloc_free(alloc, directory);
            return 0;
        }
        wchar_t *wide = galloc_alloc(alloc, (size_t)length * sizeof(*wide));
        if (!pattern || !wide) {
            galloc_free(alloc, pattern);
            galloc_free(alloc, wide);
            galloc_free(alloc, directory);
            return 0;
        }
        int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pattern, -1, wide, length);
        galloc_free(alloc, pattern);
        WIN32_FIND_DATAW data;
        HANDLE handle = converted ? FindFirstFileW(wide, &data) : INVALID_HANDLE_VALUE;
        galloc_free(alloc, wide);
        galloc_free(alloc, directory);
        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }
        found = 1;
        do {
            int utf8_length =
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, NULL, 0, NULL, NULL);
            if (utf8_length <= 0) {
                FindClose(handle);
                return 0;
            }
            char *name = galloc_alloc(alloc, (size_t)utf8_length);
            if (!name) {
                FindClose(handle);
                return 0;
            }
            int converted_name =
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, name, utf8_length, NULL, NULL);
            int result = converted_name ? gvfs__vfs_vfs_visit_entry(alloc, path, name, visit, user) : -1;
            galloc_free(alloc, name);
            if (result <= 0) {
                FindClose(handle);
                return result == 0;
            }
        } while (FindNextFileW(handle, &data));
        DWORD error = GetLastError();
        FindClose(handle);
        if (error != ERROR_NO_MORE_FILES) {
            return 0;
        }
#else
        DIR *dir = opendir(directory);
        galloc_free(alloc, directory);
        if (!dir) {
            continue;
        }
        found = 1;
        for (;;) {
            errno = 0;
            struct dirent *entry = readdir(dir);
            if (!entry) {
                int failed = errno != 0;
                closedir(dir);
                if (failed) {
                    return 0;
                }
                break;
            }
            int result = gvfs__vfs_vfs_visit_entry(alloc, path, entry->d_name, visit, user);
            if (result <= 0) {
                closedir(dir);
                return result == 0;
            }
        }
#endif
    }
    return found;
}

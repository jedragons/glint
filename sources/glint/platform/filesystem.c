#include "./filesystem.h"

#include <glint/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if GLINT_PLATFORM_WINDOWS
#include "./win32.h"
#else
#include <dirent.h>
#include <unistd.h>
#endif

static gfs_Result gfs__open(galloc_Allocator alloc, const char* path, FILE** out);
static bool gfs__visit_entry(const char* name, gfs_EntryFn visit, void* user);

#if GLINT_PLATFORM_WINDOWS
static gfs_Result gfs__to_wide(galloc_Allocator alloc, const char* path, wchar_t** out);
static gfs_Result gfs__from_wide(galloc_Allocator alloc, const wchar_t* path, char** out);
#endif

char* gfs_working_directory(galloc_Allocator alloc) {
#if GLINT_PLATFORM_WINDOWS
    wchar_t* wide = NULL;
    DWORD capacity = GetCurrentDirectoryW(0, NULL);
    while (capacity > 0) {
        wide = galloc_calloc(alloc, capacity, sizeof(*wide));
        if (wide == NULL) {
            goto error;
        }

        DWORD length = GetCurrentDirectoryW(capacity, wide);
        if (length == 0) {
            goto error;
        }

        if (length >= capacity) {
            galloc_free(alloc, wide);
            wide = NULL;
            capacity = length;
            continue;
        }

        char* path = NULL;
        if (gfs__from_wide(alloc, wide, &path) != GFS_OK) {
            goto error;
        }

        galloc_free(alloc, wide);
        return path;
    }

error:
    galloc_free(alloc, wide);
    return NULL;
#else
    char* path = NULL;
    size_t capacity = 256;
    while (true) {
        path = galloc_alloc(alloc, capacity);
        if (path == NULL) {
            goto error;
        }

        if (getcwd(path, capacity) != NULL) {
            return path;
        }

        if (errno != ERANGE || capacity > SIZE_MAX / 2) {
            goto error;
        }

        galloc_free(alloc, path);
        path = NULL;
        capacity *= 2;
    }

error:
    galloc_free(alloc, path);
    return NULL;
#endif
}

bool gfs_exists(galloc_Allocator alloc, const char* path) {
    if (path == NULL) {
        return false;
    }

    FILE* file = NULL;
    if (gfs__open(alloc, path, &file) != GFS_OK) {
        return false;
    }

    fclose(file);
    return true;
}

gfs_Result gfs_read(galloc_Allocator alloc, const char* path, void** out, size_t* size) {
    if (size != NULL) {
        *size = 0;
    }

    if (out == NULL) {
        return GFS_ERROR;
    }
    *out = NULL;
    if (path == NULL) {
        return GFS_ERROR;
    }

    FILE* file = NULL;
    void* data = NULL;
    gfs_Result result = gfs__open(alloc, path, &file);
    if (result != GFS_OK) {
        goto error;
    }

    result = GFS_ERROR;
    if (fseek(file, 0, SEEK_END) != 0) {
        goto error;
    }

    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        goto error;
    }

    data = galloc_alloc(alloc, length != 0 ? (size_t)length : 1);
    if (data == NULL) {
        result = GFS_OOM;
        goto error;
    }

    if (fread(data, 1, (size_t)length, file) != (size_t)length) {
        goto error;
    }

    fclose(file);
    *out = data;
    if (size != NULL) {
        *size = (size_t)length;
    }

    return GFS_OK;

error:
    galloc_free(alloc, data);
    if (file != NULL) {
        fclose(file);
    }
    return result;
}

gfs_Result gfs_list(galloc_Allocator alloc, const char* path, gfs_EntryFn visit, void* user) {
    if (path == NULL || visit == NULL) {
        return GFS_ERROR;
    }

    gfs_Result result = GFS_ERROR;
#if GLINT_PLATFORM_WINDOWS
    char* pattern = NULL;
    wchar_t* wide = NULL;
    char* name = NULL;
    HANDLE handle = INVALID_HANDLE_VALUE;
    size_t bytes = strlen(path);
    if (bytes > SIZE_MAX - 3) {
        goto error;
    }

    pattern = galloc_alloc(alloc, bytes + 3);
    if (pattern == NULL) {
        result = GFS_OOM;
        goto error;
    }

    memcpy(pattern, path, bytes);
    memcpy(pattern + bytes, "/*", 3);
    result = gfs__to_wide(alloc, pattern, &wide);
    if (result != GFS_OK) {
        goto error;
    }

    galloc_free(alloc, pattern);
    pattern = NULL;
    WIN32_FIND_DATAW data;
    handle = FindFirstFileW(wide, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        result = error == ERROR_NOT_ENOUGH_MEMORY || error == ERROR_OUTOFMEMORY ? GFS_OOM : GFS_UNAVAILABLE;
        goto error;
    }

    galloc_free(alloc, wide);
    wide = NULL;
    bool keep_listing = true;
    do {
        result = gfs__from_wide(alloc, data.cFileName, &name);
        if (result != GFS_OK) {
            goto error;
        }

        keep_listing = gfs__visit_entry(name, visit, user);
        galloc_free(alloc, name);
        name = NULL;
        if (!keep_listing) {
            break;
        }
    } while (FindNextFileW(handle, &data) != 0);

    if (keep_listing) {
        DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_FILES) {
            result = error == ERROR_NOT_ENOUGH_MEMORY || error == ERROR_OUTOFMEMORY ? GFS_OOM : GFS_ERROR;
            goto error;
        }
    }

    result = GFS_OK;

error:
    galloc_free(alloc, name);
    galloc_free(alloc, wide);
    galloc_free(alloc, pattern);
    if (handle != INVALID_HANDLE_VALUE) {
        FindClose(handle);
    }
#else
    (void)alloc;
    DIR* directory = opendir(path);
    if (directory == NULL) {
        result = errno == ENOMEM ? GFS_OOM : GFS_UNAVAILABLE;
        goto error;
    }

    while (true) {
        errno = 0;
        struct dirent* entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) {
                result = errno == ENOMEM ? GFS_OOM : GFS_ERROR;
                goto error;
            }
            break;
        }

        if (!gfs__visit_entry(entry->d_name, visit, user)) {
            break;
        }
    }

    result = GFS_OK;

error:
    if (directory != NULL) {
        closedir(directory);
    }
#endif
    return result;
}

static gfs_Result gfs__open(galloc_Allocator alloc, const char* path, FILE** out) {
    gfs_Result result = GFS_ERROR;

#if GLINT_PLATFORM_WINDOWS
    wchar_t* wide = NULL;
    result = gfs__to_wide(alloc, path, &wide);
    if (result != GFS_OK) {
        goto error;
    }

    *out = _wfopen(wide, L"rb");
#else
    (void)alloc;
    *out = fopen(path, "rb");
#endif

    if (*out == NULL) {
        result = errno == ENOMEM ? GFS_OOM : GFS_UNAVAILABLE;
        goto error;
    }

    result = GFS_OK;

error:
#if GLINT_PLATFORM_WINDOWS
    galloc_free(alloc, wide);
#endif

    return result;
}

static bool gfs__visit_entry(const char* name, gfs_EntryFn visit, void* user) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return true;
    }

    return visit(name, user);
}

#if GLINT_PLATFORM_WINDOWS
static gfs_Result gfs__to_wide(galloc_Allocator alloc, const char* path, wchar_t** out) {
    *out = NULL;
    wchar_t* wide = NULL;
    gfs_Result result = GFS_ERROR;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count <= 0) {
        goto error;
    }

    wide = galloc_calloc(alloc, (size_t)count, sizeof(*wide));
    if (wide == NULL) {
        result = GFS_OOM;
        goto error;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) == 0) {
        goto error;
    }

    *out = wide;
    return GFS_OK;

error:
    galloc_free(alloc, wide);
    return result;
}

static gfs_Result gfs__from_wide(galloc_Allocator alloc, const wchar_t* path, char** out) {
    *out = NULL;
    char* utf8 = NULL;
    gfs_Result result = GFS_ERROR;
    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) {
        goto error;
    }

    utf8 = galloc_alloc(alloc, (size_t)bytes);
    if (utf8 == NULL) {
        result = GFS_OOM;
        goto error;
    }

    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, utf8, bytes, NULL, NULL) == 0) {
        goto error;
    }

    *out = utf8;
    return GFS_OK;

error:
    galloc_free(alloc, utf8);
    return result;
}

#endif

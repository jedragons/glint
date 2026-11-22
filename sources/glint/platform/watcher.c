#include <glint/config.h>

#if !GLINT_PLATFORM_WINDOWS && !GLINT_PLATFORM_MACOS
#define _POSIX_C_SOURCE 200809L
#endif
#include "./watcher.h"

#include <stdint.h>
#include <string.h>

#if GLINT_PLATFORM_WINDOWS
#include <glint/platform/win32.h>
#else
#include <sys/stat.h>
#endif

typedef struct gwatch__FileState {
    uint64_t size;
    uint64_t modified;
    uint64_t modified_subsec;
    uint64_t identity;
    bool exists;
} gwatch__FileState;

typedef struct gwatch__WatchedFile {
    char* path;
#if GLINT_PLATFORM_WINDOWS
    wchar_t* wide_path;
#endif
    gwatch__FileState state;
} gwatch__WatchedFile;

struct gwatch_Watcher {
    gwatch__WatchedFile* files;
    size_t count;
    size_t capacity;
};

static gwatch__FileState gwatch__file_watcher_read_state(const gwatch__WatchedFile* file);
static bool gwatch__file_watcher_state_changed(gwatch__FileState a, gwatch__FileState b);

gwatch_Watcher* gwatch_create(galloc_Allocator alloc) {
    return galloc_calloc(alloc, 1, sizeof(gwatch_Watcher));
}

void gwatch_destroy(galloc_Allocator alloc, gwatch_Watcher* watcher) {
    if (watcher == NULL) {
        return;
    }

    for (size_t i = 0; i < watcher->count; i++) {
        galloc_free(alloc, watcher->files[i].path);
#if GLINT_PLATFORM_WINDOWS
        galloc_free(alloc, watcher->files[i].wide_path);
#endif
    }

    galloc_free(alloc, watcher->files);
    galloc_free(alloc, watcher);
}

bool gwatch_add(galloc_Allocator alloc, gwatch_Watcher* watcher, const char* path) {
    if (watcher == NULL || path == NULL) {
        return false;
    }

    bool path_empty = path[0] == '\0';
    if (path_empty) {
        return false;
    }

    for (size_t i = 0; i < watcher->count; i++) {
        if (strcmp(watcher->files[i].path, path) == 0) {
            return true;
        }
    }

    size_t length = strlen(path);
    if (length == SIZE_MAX) {
        return false;
    }

    gwatch__WatchedFile file = {0};
    file.path = galloc_alloc(alloc, length + 1);
    if (file.path == NULL) {
        return false;
    }

    memcpy(file.path, path, length + 1);
#if GLINT_PLATFORM_WINDOWS
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count == 0 || (size_t)count > SIZE_MAX / sizeof(*file.wide_path)) {
        galloc_free(alloc, file.path);
        return false;
    }

    file.wide_path = galloc_alloc(alloc, (size_t)count * sizeof(*file.wide_path));
    if (file.wide_path == NULL
        || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, file.wide_path, count) == 0) {
        galloc_free(alloc, file.wide_path);
        galloc_free(alloc, file.path);
        return false;
    }
#endif
    if (watcher->count == watcher->capacity) {
        size_t capacity = watcher->capacity != 0 ? watcher->capacity * 2 : 8;
        gwatch__WatchedFile* files = NULL;
        if (capacity >= watcher->capacity && capacity <= SIZE_MAX / sizeof(*files)) {
            files =
                galloc_realloc(alloc, watcher->files, watcher->capacity * sizeof(*files), capacity * sizeof(*files));
        }

        if (files == NULL) {
#if GLINT_PLATFORM_WINDOWS
            galloc_free(alloc, file.wide_path);
#endif
            galloc_free(alloc, file.path);
            return false;
        }

        watcher->files = files;
        watcher->capacity = capacity;
    }

    file.state = gwatch__file_watcher_read_state(&file);
    watcher->files[watcher->count] = file;
    watcher->count += 1;
    return true;
}

void gwatch_poll(gwatch_Watcher* watcher, gwatch_ChangedFn changed, void* user) {
    if (watcher == NULL || changed == NULL) {
        return;
    }
    // Do not visit paths added by the callback until the next poll.
    size_t count = watcher->count;
    for (size_t i = 0; i < count; i++) {
        gwatch__FileState current = gwatch__file_watcher_read_state(&watcher->files[i]);
        if (gwatch__file_watcher_state_changed(watcher->files[i].state, current)) {
            watcher->files[i].state = current;
            changed(watcher->files[i].path, user);
        }
    }
}

static gwatch__FileState gwatch__file_watcher_read_state(const gwatch__WatchedFile* file) {
    gwatch__FileState state = {0};
#if GLINT_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(file->wide_path, GetFileExInfoStandard, &data) != 0
        && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        state.exists = true;
        state.size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        state.modified = ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
        state.identity = ((uint64_t)data.ftCreationTime.dwHighDateTime << 32) | data.ftCreationTime.dwLowDateTime;
    }
#else
    struct stat info;
    if (stat(file->path, &info) == 0 && S_ISREG(info.st_mode)) {
        state.exists = true;
        state.size = (uint64_t)info.st_size;
#if GLINT_PLATFORM_MACOS
        state.modified = (uint64_t)info.st_mtimespec.tv_sec;
        state.modified_subsec = (uint64_t)info.st_mtimespec.tv_nsec;
#else
        state.modified = (uint64_t)info.st_mtim.tv_sec;
        state.modified_subsec = (uint64_t)info.st_mtim.tv_nsec;
#endif
        state.identity = (uint64_t)info.st_ino;
    }
#endif
    return state;
}

static bool gwatch__file_watcher_state_changed(gwatch__FileState a, gwatch__FileState b) {
    return a.exists != b.exists || a.size != b.size || a.modified != b.modified
        || a.modified_subsec != b.modified_subsec || a.identity != b.identity;
}

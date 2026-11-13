#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include <glint/assets/file_watcher.h>

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
/* Avoid collisions with raylib names in unity builds. */
#define Rectangle gwatch__Win32Rectangle
#define CloseWindow gwatch__Win32CloseWindow
#define ShowCursor gwatch__Win32ShowCursor
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#else
#include <sys/stat.h>
#endif

typedef struct gwatch__FileState {
    uint64_t size;
    uint64_t modified;
    uint64_t modified_subsec;
    uint64_t identity;
    int exists;
} gwatch__FileState;

typedef struct gwatch__WatchedFile {
    char *path;
#ifdef _WIN32
    wchar_t *wide_path;
#endif
    gwatch__FileState state;
} gwatch__WatchedFile;

struct gwatch_Watcher {
    gwatch__WatchedFile *files;
    size_t count;
    size_t capacity;
};

static gwatch__FileState gwatch__file_watcher_read_state(const gwatch__WatchedFile *file) {
    gwatch__FileState state = {0};
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(file->wide_path, GetFileExInfoStandard, &data) &&
        !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        state.exists = 1;
        state.size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        state.modified = ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
        state.identity = ((uint64_t)data.ftCreationTime.dwHighDateTime << 32) | data.ftCreationTime.dwLowDateTime;
    }
#else
    struct stat info;
    if (stat(file->path, &info) == 0 && S_ISREG(info.st_mode)) {
        state.exists = 1;
        state.size = (uint64_t)info.st_size;
#if defined(__APPLE__)
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

static int gwatch__file_watcher_state_changed(gwatch__FileState a, gwatch__FileState b) {
    return a.exists != b.exists || a.size != b.size || a.modified != b.modified ||
           a.modified_subsec != b.modified_subsec || a.identity != b.identity;
}

gwatch_Watcher *gwatch_create(galloc_Allocator alloc) {
    return galloc_calloc(alloc, 1, sizeof(gwatch_Watcher));
}

void gwatch_destroy(galloc_Allocator alloc, gwatch_Watcher *watcher) {
    if (!watcher) {
        return;
    }

    for (size_t i = 0; i < watcher->count; i++) {
        galloc_free(alloc, watcher->files[i].path);
#ifdef _WIN32
        galloc_free(alloc, watcher->files[i].wide_path);
#endif
    }
    galloc_free(alloc, watcher->files);
    galloc_free(alloc, watcher);
}

int gwatch_add(galloc_Allocator alloc, gwatch_Watcher *watcher, const char *path) {
    if (!watcher || !path || !*path) {
        return 0;
    }

    for (size_t i = 0; i < watcher->count; i++) {
        if (strcmp(watcher->files[i].path, path) == 0) {
            return 1;
        }
    }

    size_t length = strlen(path);
    if (length == SIZE_MAX) {
        return 0;
    }
    gwatch__WatchedFile file = {0};
    file.path = galloc_alloc(alloc, length + 1);
    if (!file.path) {
        return 0;
    }
    memcpy(file.path, path, length + 1);
#ifdef _WIN32
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!count || (size_t)count > SIZE_MAX / sizeof(*file.wide_path)) {
        galloc_free(alloc, file.path);
        return 0;
    }
    file.wide_path = galloc_alloc(alloc, (size_t)count * sizeof(*file.wide_path));
    if (!file.wide_path || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, file.wide_path, count)) {
        galloc_free(alloc, file.wide_path);
        galloc_free(alloc, file.path);
        return 0;
    }
#endif
    if (watcher->count == watcher->capacity) {
        size_t capacity = watcher->capacity ? watcher->capacity * 2 : 8;
        gwatch__WatchedFile *files = NULL;
        if (capacity >= watcher->capacity && capacity <= SIZE_MAX / sizeof(*files)) {
            files = galloc_realloc(alloc, watcher->files, watcher->capacity * sizeof(*files), capacity * sizeof(*files));
        }
        if (!files) {
#ifdef _WIN32
            galloc_free(alloc, file.wide_path);
#endif
            galloc_free(alloc, file.path);
            return 0;
        }
        watcher->files = files;
        watcher->capacity = capacity;
    }
    file.state = gwatch__file_watcher_read_state(&file);
    watcher->files[watcher->count++] = file;
    return 1;
}

void gwatch_poll(gwatch_Watcher *watcher, gwatch_ChangedFn changed, void *user) {
    if (!watcher || !changed) {
        return;
    }
    /* Do not visit paths added by the callback until the next poll. */
    size_t count = watcher->count;
    for (size_t i = 0; i < count; i++) {
        gwatch__FileState current = gwatch__file_watcher_read_state(&watcher->files[i]);
        if (gwatch__file_watcher_state_changed(watcher->files[i].state, current)) {
            watcher->files[i].state = current;
            changed(watcher->files[i].path, user);
        }
    }
}

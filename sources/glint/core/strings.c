#include "./strings.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool gstr__is_space(char c);

gstr_Slice gstr_Slice_from_cstr(const char* str) {
    if (str == NULL) {
        return (gstr_Slice) {0};
    }

    return (gstr_Slice) {.items = str, .count = strlen(str)};
}

gstr_Slice gstr_Slice_slice(gstr_Slice slice, size_t start, size_t count) {
    if (start > slice.count || count > slice.count - start || (slice.items == NULL && (start > 0 || count > 0))) {
        return (gstr_Slice) {0};
    }

    return (gstr_Slice) {.items = slice.items != NULL ? slice.items + start : NULL, .count = count};
}

gstr_Slice gstr_Slice_trim_left(gstr_Slice slice) {
    if (slice.items == NULL) {
        return (gstr_Slice) {0};
    }

    while (slice.count > 0 && gstr__is_space(*slice.items)) {
        slice.items++;
        slice.count--;
    }

    return slice;
}

gstr_Slice gstr_Slice_trim_right(gstr_Slice slice) {
    if (slice.items == NULL) {
        return (gstr_Slice) {0};
    }

    while (slice.count > 0 && gstr__is_space(slice.items[slice.count - 1])) {
        slice.count--;
    }

    return slice;
}

gstr_Slice gstr_Slice_trim_space(gstr_Slice slice) {
    return gstr_Slice_trim_right(gstr_Slice_trim_left(slice));
}

bool gstr_Slice_eq(gstr_Slice a, gstr_Slice b) {
    if (a.count != b.count) {
        return false;
    }
    if (a.count == 0) {
        return true;
    }
    return a.items && b.items && memcmp(a.items, b.items, a.count) == 0;
}

void gstr_Buffer_destroy(galloc_Allocator alloc, gstr_Buffer* sb) {
    galloc_free(alloc, sb->items);
    *sb = (gstr_Buffer) {0};
}

bool gstr_Buffer_reserve(galloc_Allocator alloc, gstr_Buffer* sb, size_t capacity) {
    if (capacity <= sb->capacity) {
        return true;
    }

    if (capacity == SIZE_MAX) {
        return false;
    }

    size_t next = sb->capacity != 0 ? sb->capacity : 16;
    while (next < capacity) {
        if (next > (SIZE_MAX - 1) / 2) {
            next = capacity;
            break;
        }
        next *= 2;
    }

    char* items = galloc_realloc(alloc, sb->items, sb->items != NULL ? sb->capacity + 1 : 0, next + 1);
    if (items == NULL) {
        return false;
    }

    items[sb->count] = '\0';
    sb->items = items;
    sb->capacity = next;
    return true;
}

bool gstr_Buffer_append_slice(galloc_Allocator alloc, gstr_Buffer* sb, gstr_Slice slice) {
    if (slice.count == 0) {
        return true;
    }

    if (slice.items == NULL || slice.count > SIZE_MAX - sb->count - 1) {
        return false;
    }

    // Remember offsets into this buffer before a possible reallocation.
    bool aliases = false;
    size_t offset = 0;
    if (sb->items != NULL && (uintptr_t)slice.items >= (uintptr_t)sb->items) {
        uintptr_t difference = (uintptr_t)slice.items - (uintptr_t)sb->items;
        if (difference <= sb->capacity) {
            offset = (size_t)difference;
            if (offset > sb->count || slice.count > sb->count - offset) {
                return false;
            }
            aliases = true;
        }
    }

    if (!gstr_Buffer_reserve(alloc, sb, sb->count + slice.count)) {
        return false;
    }

    const char* source = aliases ? sb->items + offset : slice.items;

    memmove(sb->items + sb->count, source, slice.count);

    sb->count += slice.count;
    sb->items[sb->count] = '\0';

    return true;
}

bool gstr_Buffer_append_cstr(galloc_Allocator alloc, gstr_Buffer* sb, const char* str) {
    if (str == NULL) {
        return false;
    }

    return gstr_Buffer_append_slice(alloc, sb, gstr_Slice_from_cstr(str));
}

bool gstr_Buffer_append_char(galloc_Allocator alloc, gstr_Buffer* sb, char c) {
    return gstr_Buffer_append_slice(alloc, sb, (gstr_Slice) {.items = &c, .count = 1});
}

bool gstr_Buffer_appendf(galloc_Allocator alloc, gstr_Buffer* sb, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool ok = gstr_Buffer_vappendf(alloc, sb, format, args);
    va_end(args);

    return ok;
}

bool gstr_Buffer_vappendf(galloc_Allocator alloc, gstr_Buffer* sb, const char* format, va_list args) {
    if (format == NULL) {
        return false;
    }

    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (length < 0 || (size_t)length > SIZE_MAX - sb->count - 1) {
        return false;
    }

    char* buffer = galloc_alloc(alloc, (size_t)length + 1);
    if (buffer == NULL) {
        return false;
    }

    va_copy(copy, args);
    int written = vsnprintf(buffer, (size_t)length + 1, format, copy);
    va_end(copy);

    bool ok = written == length
        && gstr_Buffer_append_slice(alloc, sb, (gstr_Slice) {.items = buffer, .count = (size_t)length});

    galloc_free(alloc, buffer);

    return ok;
}

gstr_Slice gstr_Buffer_slice(const gstr_Buffer* sb) {
    return (gstr_Slice) {.items = sb->items, .count = sb->count};
}

const char* gstr_Buffer_cstr(const gstr_Buffer* sb) {
    return sb->items != NULL ? sb->items : "";
}

void gstr_Buffer_clear(gstr_Buffer* sb) {
    sb->count = 0;

    if (sb->items != NULL) {
        sb->items[0] = '\0';
    }
}

static bool gstr__is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

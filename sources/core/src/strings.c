#include <glint/strings.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

gstr_View gstr_View_from_cstr(const char *str) {
    if (!str) {
        return (gstr_View) {0};
    }
    return (gstr_View) {.items = str, .count = strlen(str)};
}

gstr_View gstr_View_slice(gstr_View view, size_t start, size_t count) {
    if (start > view.count || count > view.count - start || (!view.items && (start || count))) {
        return (gstr_View) {0};
    }
    return (gstr_View) {.items = view.items ? view.items + start : NULL, .count = count};
}

static bool gstr__is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

gstr_View gstr_View_trim_space(gstr_View view) {
    if (!view.items) {
        return (gstr_View) {0};
    }
    while (view.count && gstr__is_space(*view.items)) {
        view.items++;
        view.count--;
    }
    while (view.count && gstr__is_space(view.items[view.count - 1])) {
        view.count--;
    }
    return view;
}

bool gstr_View_eq(gstr_View a, gstr_View b) {
    if (a.count != b.count) {
        return false;
    }
    if (!a.count) {
        return true;
    }
    return a.items && b.items && memcmp(a.items, b.items, a.count) == 0;
}

void gstr_Builder_init(gstr_Builder *sb) {
    *sb = (gstr_Builder) {0};
}

void gstr_Builder_destroy(galloc_Allocator alloc, gstr_Builder *sb) {
    galloc_free(alloc, sb->items);
    *sb = (gstr_Builder) {0};
}

bool gstr_Builder_reserve(galloc_Allocator alloc, gstr_Builder *sb, size_t capacity) {
    if (capacity <= sb->capacity) {
        return true;
    }
    if (capacity == SIZE_MAX) {
        return false;
    }
    size_t next = sb->capacity ? sb->capacity : 16;
    while (next < capacity) {
        if (next > (SIZE_MAX - 1) / 2) {
            next = capacity;
            break;
        }
        next *= 2;
    }
    char *items = galloc_realloc(alloc, sb->items, sb->items ? sb->capacity + 1 : 0, next + 1);
    if (!items) {
        return false;
    }
    items[sb->count] = '\0';
    sb->items = items;
    sb->capacity = next;
    return true;
}

bool gstr_Builder_append_sv(galloc_Allocator alloc, gstr_Builder *sb, gstr_View view) {
    if (!view.count) {
        return true;
    }
    if (!view.items || view.count > SIZE_MAX - sb->count - 1) {
        return false;
    }
    /* Remember offsets into this builder before a possible reallocation. */
    bool aliases = false;
    size_t offset = 0;
    if (sb->items && (uintptr_t)view.items >= (uintptr_t)sb->items) {
        uintptr_t difference = (uintptr_t)view.items - (uintptr_t)sb->items;
        if (difference <= sb->capacity) {
            offset = (size_t)difference;
            if (offset > sb->count || view.count > sb->count - offset) {
                return false;
            }
            aliases = true;
        }
    }
    if (!gstr_Builder_reserve(alloc, sb, sb->count + view.count)) {
        return false;
    }
    const char *source = aliases ? sb->items + offset : view.items;
    memmove(sb->items + sb->count, source, view.count);
    sb->count += view.count;
    sb->items[sb->count] = '\0';
    return true;
}

bool gstr_Builder_append_cstr(galloc_Allocator alloc, gstr_Builder *sb, const char *str) {
    if (!str) {
        return false;
    }
    return gstr_Builder_append_sv(alloc, sb, gstr_View_from_cstr(str));
}

bool gstr_Builder_append_char(galloc_Allocator alloc, gstr_Builder *sb, char c) {
    return gstr_Builder_append_sv(alloc, sb, (gstr_View) {.items = &c, .count = 1});
}

bool gstr_Builder_appendf(galloc_Allocator alloc, gstr_Builder *sb, const char *format, ...) {
    if (!format) {
        return false;
    }
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0 || (size_t)length > SIZE_MAX - sb->count - 1) {
        va_end(args);
        return false;
    }
    char *buffer = galloc_alloc(alloc, (size_t)length + 1);
    if (!buffer) {
        va_end(args);
        return false;
    }
    int written = vsnprintf(buffer, (size_t)length + 1, format, args);
    va_end(args);
    bool ok = written == length && gstr_Builder_append_sv(alloc, sb, (gstr_View) {.items = buffer, .count = (size_t)length});
    galloc_free(alloc, buffer);
    return ok;
}

gstr_View gstr_Builder_view(const gstr_Builder *sb) {
    return (gstr_View) {.items = sb->items, .count = sb->count};
}

const char *gstr_Builder_cstr(const gstr_Builder *sb) {
    return sb->items ? sb->items : "";
}

void gstr_Builder_clear(gstr_Builder *sb) {
    sb->count = 0;
    if (sb->items) {
        sb->items[0] = '\0';
    }
}

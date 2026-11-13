#ifndef GSTR_STRINGS_H
#define GSTR_STRINGS_H

#include <glint/alloc.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* String views are borrowed byte ranges and need not be NUL-terminated. */
typedef struct gstr_View {
    const char *items;
    size_t count;
} gstr_View;

gstr_View gstr_View_from_cstr(const char *str);
gstr_View gstr_View_slice(gstr_View view, size_t start, size_t count);
gstr_View gstr_View_trim_space(gstr_View view);
bool gstr_View_eq(gstr_View a, gstr_View b);

typedef struct gstr_Builder {
    char *items;
    size_t count;
    size_t capacity;
} gstr_Builder;

/* Use the same allocator for every operation on a builder until destruction.
 * The builder owns a trailing NUL that is not included in count or capacity.
 * An empty, unallocated builder has items == NULL; gstr_Builder_cstr returns "" for it. */
void gstr_Builder_init(gstr_Builder *sb);
void gstr_Builder_destroy(galloc_Allocator alloc, gstr_Builder *sb);
bool gstr_Builder_reserve(galloc_Allocator alloc, gstr_Builder *sb, size_t capacity);
bool gstr_Builder_append_sv(galloc_Allocator alloc, gstr_Builder *sb, gstr_View view);
bool gstr_Builder_append_cstr(galloc_Allocator alloc, gstr_Builder *sb, const char *str);
bool gstr_Builder_append_char(galloc_Allocator alloc, gstr_Builder *sb, char c);
bool gstr_Builder_appendf(galloc_Allocator alloc, gstr_Builder *sb, const char *format, ...);
gstr_View gstr_Builder_view(const gstr_Builder *sb);
const char *gstr_Builder_cstr(const gstr_Builder *sb);
void gstr_Builder_clear(gstr_Builder *sb);

#ifdef __cplusplus
}
#endif
#endif

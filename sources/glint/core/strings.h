#pragma once

#include <glint/core/alloc.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Non-owning byte range; not necessarily NUL-terminated.
typedef struct gstr_Slice {
    const char* items;
    size_t count;
} gstr_Slice;

/// Wrap NUL-terminated string into slice
gstr_Slice gstr_Slice_from_cstr(const char* str);

gstr_Slice gstr_Slice_slice(gstr_Slice slice, size_t start, size_t count);

/// Trim ASCII whitespace from the left side of a `slice`.
gstr_Slice gstr_Slice_trim_left(gstr_Slice slice);

/// Trim ASCII whitespace from the right side of `slice`.
gstr_Slice gstr_Slice_trim_right(gstr_Slice slice);

/// Trim ASCII whitespace from both sides of a slice.
gstr_Slice gstr_Slice_trim_space(gstr_Slice slice);

bool gstr_Slice_eq(gstr_Slice a, gstr_Slice b);

/// Owning string buffer; zero initialization is ready to use.
///
/// # Ownership
///
/// Use the same allocator until destruction. The trailing NUL is excluded from
/// `count` and `capacity`.
typedef struct gstr_Buffer {
    char* items;
    size_t count;
    size_t capacity;
} gstr_Buffer;

void gstr_Buffer_destroy(galloc_Allocator alloc, gstr_Buffer* sb);

/// Reserve at least `capacity` bytes. Capacity grows geometrically.
bool gstr_Buffer_reserve(galloc_Allocator alloc, gstr_Buffer* sb, size_t capacity);

bool gstr_Buffer_append_slice(galloc_Allocator alloc, gstr_Buffer* sb, gstr_Slice slice);

bool gstr_Buffer_append_cstr(galloc_Allocator alloc, gstr_Buffer* sb, const char* str);

bool gstr_Buffer_append_char(galloc_Allocator alloc, gstr_Buffer* sb, char c);

bool gstr_Buffer_appendf(galloc_Allocator alloc, gstr_Buffer* sb, const char* format, ...);

/// Append formatted text to the end of `sb` without consuming `args`.
bool gstr_Buffer_vappendf(galloc_Allocator alloc, gstr_Buffer* sb, const char* format, va_list args);

gstr_Slice gstr_Buffer_slice(const gstr_Buffer* sb);

const char* gstr_Buffer_cstr(const gstr_Buffer* sb);

/// Clear `sb` without deallocating memory
void gstr_Buffer_clear(gstr_Buffer* sb);

#ifdef __cplusplus
}
#endif

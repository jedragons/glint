#ifndef GJSON_CORE_JSON_H
#define GJSON_CORE_JSON_H

#include <stddef.h>

#include <glint/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gjson_Type {
    GJSON_NULL,
    GJSON_BOOL,
    GJSON_NUMBER,
    GJSON_STRING,
    GJSON_ARRAY,
    GJSON_OBJECT
} gjson_Type;

typedef struct gjson_Value gjson_Value;

struct gjson_Value {
    gjson_Type type;
    char *key; /* object member name, otherwise NULL */
    size_t key_length;
    gjson_Value *next;

    union {
        int boolean;
        double number;

        struct {
            char *data;
            size_t length;
        } string;

        struct {
            gjson_Value *first;
            size_t length;
        } children;
    } as;
};

/* Parse exactly length bytes, with nesting limited to 128 levels. Numbers use
 * double precision; values outside its range may round to infinity or zero.
 * On failure, error_offset receives the error position when non-NULL.
 * The returned tree is owned by the caller. Free it with the same allocator. */
gjson_Value *gjson_parse(galloc_Allocator alloc, const char *input, size_t length, size_t *error_offset);
void gjson_Value_free(galloc_Allocator alloc, gjson_Value *value);
/* Object lookup compares the full member name, including embedded NUL bytes. */
const gjson_Value *gjson_Value_get_n(const gjson_Value *object, const char *key, size_t length);
const gjson_Value *gjson_Value_get(const gjson_Value *object, const char *key);
const gjson_Value *gjson_Value_at(const gjson_Value *array, size_t index);

#ifdef __cplusplus
}
#endif
#endif

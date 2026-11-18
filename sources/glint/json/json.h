#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <glint/core/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gjson_Type {
    GJSON_NULL,
    GJSON_BOOL,
    GJSON_NUMBER,
    GJSON_STRING,
    GJSON_ARRAY,
    GJSON_OBJECT,
} gjson_Type;

typedef struct gjson_Value gjson_Value;

struct gjson_Value {
    gjson_Type type;
    /// object member name, otherwise NULL
    char* key;
    size_t key_length;
    gjson_Value* next;

    union {
        bool boolean;
        double number;

        struct {
            char* data;
            size_t length;
        } string;

        struct {
            gjson_Value* first;
            size_t length;
        } children;
    } as;
};

/// Parse a JSON value from a byte range
gjson_Value* gjson_parse(galloc_Allocator alloc, const char* input, size_t length, size_t* error_offset);

void gjson_Value_free(galloc_Allocator alloc, gjson_Value* value);

/// Find an object member by a byte-range key
const gjson_Value* gjson_Value_get_n(const gjson_Value* object, const char* key, size_t length);

const gjson_Value* gjson_Value_get(const gjson_Value* object, const char* key);

const gjson_Value* gjson_Value_at(const gjson_Value* array, size_t index);

#ifdef __cplusplus
}
#endif

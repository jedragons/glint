#include "./json.h"

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct gjson__Parser {
    const uint8_t* input;
    size_t length;
    size_t pos;
} gjson__Parser;

static void gjson__json_space(gjson__Parser* p);
static bool gjson__json_hex4(gjson__Parser* p, uint32_t* out);
static bool gjson__json_utf8(gjson__Parser* p, char* s, size_t* n, uint8_t c);
static void gjson__json_encode(char* s, size_t* n, uint32_t cp);
static char* gjson__json_string(galloc_Allocator alloc, gjson__Parser* p, size_t* length);
static bool gjson__json_digit(gjson__Parser* p);
static bool gjson__json_number(galloc_Allocator alloc, gjson__Parser* p, double* out);
static gjson_Value* gjson__json_value(galloc_Allocator alloc, gjson__Parser* p, uint32_t depth);
static bool gjson__json_literal(gjson__Parser* p, const char* literal, size_t length);

void gjson_Value_free(galloc_Allocator alloc, gjson_Value* v) {
    while (v != NULL) {
        gjson_Value* next = v->next;
        galloc_free(alloc, v->key);
        if (v->type == GJSON_STRING) {
            galloc_free(alloc, v->as.string.data);
        }

        if (v->type == GJSON_ARRAY || v->type == GJSON_OBJECT) {
            gjson_Value_free(alloc, v->as.children.first);
        }

        galloc_free(alloc, v);
        v = next;
    }
}

gjson_Value* gjson_parse(galloc_Allocator alloc, const char* input, size_t length, size_t* error_offset) {
    if (error_offset != NULL) {
        *error_offset = 0;
    }

    if (input == NULL) {
        return NULL;
    }

    gjson__Parser p = {
        .input = (const uint8_t*)input,
        .length = length,
        .pos = 0,
    };

    gjson_Value* v = gjson__json_value(alloc, &p, 0);
    gjson__json_space(&p);
    bool parse_failed = v == NULL;
    bool trailing_input = p.pos != length;
    if (parse_failed || trailing_input) {
        gjson_Value_free(alloc, v);
        if (error_offset != NULL) {
            *error_offset = p.pos;
        }

        return NULL;
    }

    return v;
}

const gjson_Value* gjson_Value_get_n(const gjson_Value* v, const char* key, size_t length) {
    bool invalid_object = v == NULL || v->type != GJSON_OBJECT;
    if (invalid_object || key == NULL) {
        return NULL;
    }

    for (gjson_Value* child = v->as.children.first; child != NULL; child = child->next) {
        if (child->key_length == length && memcmp(child->key, key, length) == 0) {
            return child;
        }
    }

    return NULL;
}

const gjson_Value* gjson_Value_get(const gjson_Value* v, const char* key) {
    return key != NULL ? gjson_Value_get_n(v, key, strlen(key)) : NULL;
}

const gjson_Value* gjson_Value_at(const gjson_Value* v, size_t index) {
    if (v == NULL || v->type != GJSON_ARRAY) {
        return NULL;
    }

    const gjson_Value* child = v->as.children.first;
    while (child != NULL && index > 0) {
        child = child->next;
        index -= 1;
    }

    return child;
}

static void gjson__json_space(gjson__Parser* p) {
    while (p->pos < p->length) {
        uint8_t c = p->input[p->pos];
        bool whitespace = c == ' ' || c == '\t' || c == '\r' || c == '\n';
        if (!whitespace) {
            break;
        }

        p->pos += 1;
    }
}

static bool gjson__json_hex4(gjson__Parser* p, uint32_t* out) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (p->pos >= p->length) {
            return false;
        }
        uint8_t c = p->input[p->pos++];
        value <<= 4;

        if (c >= '0' && c <= '9') {
            value |= (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= (uint32_t)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= (uint32_t)(c - 'A' + 10);
        } else {
            return false;
        }
    }

    *out = value;
    return true;
}

static bool gjson__json_utf8(gjson__Parser* p, char* s, size_t* n, uint8_t c) {
    if (c < 0x80) {
        s[(*n)++] = (char)c;
        return true;
    }

    uint32_t cp = 0;
    uint32_t min = 0;
    uint8_t count = 0;

    if (c >= 0xc2 && c <= 0xdf) {
        cp = c & 31u;
        count = 1;
        min = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
        cp = c & 15u;
        count = 2;
        min = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
        cp = c & 7u;
        count = 3;
        min = 0x10000;
    } else {
        return false;
    }

    s[(*n)++] = (char)c;

    for (uint8_t i = 0; i < count; ++i) {
        if (p->pos >= p->length) {
            return false;
        }

        c = p->input[p->pos++];
        if ((c & 0xc0) != 0x80) {
            return false;
        }

        s[(*n)++] = (char)c;
        cp = (cp << 6) | (c & 63u);
    }

    bool in_range = cp >= min && cp <= 0x10ffff;
    bool surrogate = cp >= 0xd800 && cp <= 0xdfff;
    return in_range && !surrogate;
}

static void gjson__json_encode(char* s, size_t* n, uint32_t cp) {
    if (cp < 0x80) {
        s[(*n)++] = (char)cp;
    } else if (cp < 0x800) {
        s[(*n)++] = (char)(0xc0 | (cp >> 6));
        s[(*n)++] = (char)(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
        s[(*n)++] = (char)(0xe0 | (cp >> 12));
        s[(*n)++] = (char)(0x80 | ((cp >> 6) & 63));
        s[(*n)++] = (char)(0x80 | (cp & 63));
    } else {
        s[(*n)++] = (char)(0xf0 | (cp >> 18));
        s[(*n)++] = (char)(0x80 | ((cp >> 12) & 63));
        s[(*n)++] = (char)(0x80 | ((cp >> 6) & 63));
        s[(*n)++] = (char)(0x80 | (cp & 63));
    }
}

static char* gjson__json_string(galloc_Allocator alloc, gjson__Parser* p, size_t* length) {
    bool has_opening_quote = p->pos < p->length && p->input[p->pos++] == '"';
    if (!has_opening_quote) {
        return NULL;
    }

    size_t end = p->pos;
    while (end < p->length && p->input[end] != '"') {
        char current = p->input[end];
        end++;
        if (current == '\\' && end < p->length) {
            end += 1;
        }
    }

    bool missing_closing_quote = end == p->length;
    bool allocation_overflows = end - p->pos == SIZE_MAX;
    if (missing_closing_quote || allocation_overflows) {
        return NULL;
    }

    char* s = (char*)galloc_alloc(alloc, end - p->pos + 1);
    if (s == NULL) {
        return NULL;
    }

    size_t n = 0;
    while (p->pos < p->length) {
        uint8_t c = p->input[p->pos++];
        if (c == '"') {
            s[n] = '\0';
            *length = n;
            return s;
        }

        if (c < 0x20) {
            break;
        }

        if (c == '\\') {
            uint32_t cp;
            uint32_t low;
            if (p->pos >= p->length) {
                break;
            }
            c = p->input[p->pos++];
            switch (c) {
                case '"':
                case '\\':
                case '/': {
                    s[n++] = (char)c;
                    break;
                }
                case 'b': {
                    s[n++] = '\b';
                    break;
                }
                case 'f': {
                    s[n++] = '\f';
                    break;
                }
                case 'n': {
                    s[n++] = '\n';
                    break;
                }
                case 'r': {
                    s[n++] = '\r';
                    break;
                }
                case 't': {
                    s[n++] = '\t';
                    break;
                }
                case 'u': {
                    if (!gjson__json_hex4(p, &cp)) {
                        goto error;
                    }

                    bool high_surrogate = cp >= 0xd800 && cp <= 0xdbff;
                    bool low_surrogate = cp >= 0xdc00 && cp <= 0xdfff;
                    if (high_surrogate) {
                        bool has_low_escape =
                            p->length - p->pos >= 6 && p->input[p->pos] == '\\' && p->input[p->pos + 1] == 'u';
                        if (!has_low_escape) {
                            goto error;
                        }

                        p->pos += 2;
                        if (!gjson__json_hex4(p, &low)) {
                            goto error;
                        }

                        bool valid_low_surrogate = low >= 0xdc00 && low <= 0xdfff;
                        if (!valid_low_surrogate) {
                            goto error;
                        }

                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                    } else if (low_surrogate) {
                        goto error;
                    }

                    gjson__json_encode(s, &n, cp);
                    break;
                }
                default:
                    goto error;
            }
        } else if (!gjson__json_utf8(p, s, &n, c)) {
            goto error;
        }
    }

error:
    galloc_free(alloc, s);
    return NULL;
}

static bool gjson__json_digit(gjson__Parser* p) {
    return p->pos < p->length && p->input[p->pos] >= '0' && p->input[p->pos] <= '9';
}

static bool gjson__json_number(galloc_Allocator alloc, gjson__Parser* p, double* out) {
    size_t start = p->pos;
    if (p->input[p->pos] == '-') {
        p->pos += 1;
    }

    if (!gjson__json_digit(p)) {
        return false;
    }

    if (p->input[p->pos] == '0') {
        p->pos += 1;
    } else {
        while (gjson__json_digit(p)) {
            p->pos += 1;
        }
    }

    if (p->pos < p->length && p->input[p->pos] == '.') {
        p->pos += 1;
        if (!gjson__json_digit(p)) {
            return false;
        }

        while (gjson__json_digit(p)) {
            p->pos += 1;
        }
    }

    bool has_exponent = p->pos < p->length && (p->input[p->pos] == 'e' || p->input[p->pos] == 'E');
    if (has_exponent) {
        p->pos += 1;
        bool has_sign = p->pos < p->length && (p->input[p->pos] == '+' || p->input[p->pos] == '-');
        if (has_sign) {
            p->pos += 1;
        }

        if (!gjson__json_digit(p)) {
            return false;
        }

        while (gjson__json_digit(p)) {
            p->pos += 1;
        }
    }

    size_t n = p->pos - start;
    const char* point = localeconv()->decimal_point;
    size_t point_length = strlen(point);
    bool invalid_point = point_length == 0;
    bool allocation_overflows = n > SIZE_MAX - point_length;
    if (invalid_point || allocation_overflows) {
        return false;
    }

    char* buf = (char*)galloc_alloc(alloc, n + point_length);
    if (buf == NULL) {
        return false;
    }

    size_t i = 0;
    size_t j = 0;
    while (i < n) {
        if (p->input[start + i] == '.') {
            memcpy(buf + j, point, point_length);
            j += point_length;
        } else {
            buf[j++] = (char)p->input[start + i];
        }
        i += 1;
    }

    buf[j] = '\0';
    char* end;
    *out = strtod(buf, &end);
    if (end != buf + j) {
        galloc_free(alloc, buf);
        return false;
    }

    galloc_free(alloc, buf);
    return true;
}

static gjson_Value* gjson__json_value(galloc_Allocator alloc, gjson__Parser* p, uint32_t depth) {
    gjson__json_space(p);
    bool too_deep = depth > 128;
    bool no_input = p->pos >= p->length;
    if (too_deep || no_input) {
        return NULL;
    }

    gjson_Value* v = (gjson_Value*)galloc_calloc(alloc, 1, sizeof(*v));

    if (v == NULL) {
        return NULL;
    }

    uint8_t c = p->input[p->pos];
    bool is_container = c == '[' || c == '{';
    bool is_number = c == '-' || (c >= '0' && c <= '9');

    if (c == '"') {
        v->type = GJSON_STRING;
        v->as.string.data = gjson__json_string(alloc, p, &v->as.string.length);
        if (v->as.string.data == NULL) {
            goto error;
        }
    } else if (is_container) {
        uint8_t close = c == '[' ? ']' : '}';
        v->type = c == '[' ? GJSON_ARRAY : GJSON_OBJECT;
        ++p->pos;
        gjson__json_space(p);
        gjson_Value** tail = &v->as.children.first;

        if (p->pos < p->length && p->input[p->pos] == close) {
            ++p->pos;
            return v;
        }

        while (true) {
            char* key = NULL;
            size_t key_len = 0;
            if (v->type == GJSON_OBJECT) {
                key = gjson__json_string(alloc, p, &key_len);
                if (key == NULL) {
                    goto error;
                }

                gjson__json_space(p);
                bool has_colon = p->pos < p->length && p->input[p->pos++] == ':';
                if (!has_colon) {
                    galloc_free(alloc, key);
                    goto error;
                }
            }

            gjson_Value* child = gjson__json_value(alloc, p, depth + 1);
            if (child == NULL) {
                galloc_free(alloc, key);
                goto error;
            }

            child->key = key;
            child->key_length = key_len;
            *tail = child;
            tail = &child->next;
            ++v->as.children.length;
            gjson__json_space(p);
            if (p->pos >= p->length) {
                goto error;
            }

            c = p->input[p->pos++];

            if (c == close) {
                return v;
            }

            if (c != ',') {
                goto error;
            }

            gjson__json_space(p);
        }
    } else if (is_number) {
        v->type = GJSON_NUMBER;
        if (!gjson__json_number(alloc, p, &v->as.number)) {
            goto error;
        }
    } else if (gjson__json_literal(p, "null", 4)) {
        v->type = GJSON_NULL;
    } else if (gjson__json_literal(p, "true", 4)) {
        v->type = GJSON_BOOL;
        v->as.boolean = true;
    } else if (gjson__json_literal(p, "false", 5)) {
        v->type = GJSON_BOOL;
    } else {
        goto error;
    }

    return v;

error:
    gjson_Value_free(alloc, v);
    return NULL;
}

static bool gjson__json_literal(gjson__Parser* p, const char* literal, size_t length) {
    bool enough_input = p->length - p->pos >= length;
    bool matches = enough_input && memcmp(p->input + p->pos, literal, length) == 0;
    if (!matches) {
        return false;
    }

    p->pos += length;
    return true;
}

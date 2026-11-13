#include <glint/json.h>

#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct gjson__Parser {
    const unsigned char *input;
    size_t length, pos;
} gjson__Parser;

static void gjson__json_space(gjson__Parser *p) {
    while (p->pos < p->length) {
        unsigned char c = p->input[p->pos];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        ++p->pos;
    }
}

void gjson_Value_free(galloc_Allocator alloc, gjson_Value *v) {
    while (v) {
        gjson_Value *next = v->next;
        galloc_free(alloc, v->key);
        if (v->type == GJSON_STRING)
            galloc_free(alloc, v->as.string.data);
        if (v->type == GJSON_ARRAY || v->type == GJSON_OBJECT)
            gjson_Value_free(alloc, v->as.children.first);
        galloc_free(alloc, v);
        v = next;
    }
}

static int gjson__json_hex4(gjson__Parser *p, unsigned *out) {
    unsigned value = 0;
    int i;
    for (i = 0; i < 4; ++i) {
        unsigned char c;
        if (p->pos >= p->length)
            return 0;
        c = p->input[p->pos++];
        value <<= 4;
        if (c >= '0' && c <= '9')
            value |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f')
            value |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            value |= (unsigned)(c - 'A' + 10);
        else
            return 0;
    }
    *out = value;
    return 1;
}

/* Input UTF-8 is validated so that all successful strings contain Unicode. */
static int gjson__json_utf8(gjson__Parser *p, char *s, size_t *n, unsigned char c) {
    unsigned cp, min;
    int count, i;
    if (c < 0x80) {
        s[(*n)++] = (char)c;
        return 1;
    }
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
    } else
        return 0;
    s[(*n)++] = (char)c;
    for (i = 0; i < count; ++i) {
        if (p->pos >= p->length)
            return 0;
        c = p->input[p->pos++];
        if ((c & 0xc0) != 0x80)
            return 0;
        s[(*n)++] = (char)c;
        cp = (cp << 6) | (c & 63u);
    }
    return cp >= min && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}

static void gjson__json_encode(char *s, size_t *n, unsigned cp) {
    if (cp < 0x80)
        s[(*n)++] = (char)cp;
    else if (cp < 0x800) {
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

static char *gjson__json_string(galloc_Allocator alloc, gjson__Parser *p, size_t *length) {
    char *s;
    size_t n = 0, end;
    if (p->pos >= p->length || p->input[p->pos++] != '"')
        return NULL;
    /* The decoded string cannot exceed the encoded bytes before the closing quote. */
    end = p->pos;
    while (end < p->length && p->input[end] != '"') {
        if (p->input[end++] == '\\' && end < p->length)
            ++end;
    }
    if (end == p->length || end - p->pos == SIZE_MAX)
        return NULL;
    s = (char *)galloc_alloc(alloc, end - p->pos + 1);
    if (!s)
        return NULL;
    while (p->pos < p->length) {
        unsigned char c = p->input[p->pos++];
        if (c == '"') {
            s[n] = '\0';
            *length = n;
            return s;
        }
        if (c < 0x20)
            break;
        if (c == '\\') {
            unsigned cp, low;
            if (p->pos >= p->length)
                break;
            c = p->input[p->pos++];
            switch (c) {
                case '"':
                case '\\':
                case '/':
                    s[n++] = (char)c;
                    break;
                case 'b':
                    s[n++] = '\b';
                    break;
                case 'f':
                    s[n++] = '\f';
                    break;
                case 'n':
                    s[n++] = '\n';
                    break;
                case 'r':
                    s[n++] = '\r';
                    break;
                case 't':
                    s[n++] = '\t';
                    break;
                case 'u':
                    if (!gjson__json_hex4(p, &cp))
                        goto fail;
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        if (p->length - p->pos < 6 || p->input[p->pos] != '\\' || p->input[p->pos + 1] != 'u')
                            goto fail;
                        p->pos += 2;
                        if (!gjson__json_hex4(p, &low) || low < 0xdc00 || low > 0xdfff)
                            goto fail;
                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                    } else if (cp >= 0xdc00 && cp <= 0xdfff)
                        goto fail;
                    gjson__json_encode(s, &n, cp);
                    break;
                default:
                    goto fail;
            }
        } else if (!gjson__json_utf8(p, s, &n, c))
            goto fail;
    }
fail:
    galloc_free(alloc, s);
    return NULL;
}

static int gjson__json_digit(gjson__Parser *p) {
    return p->pos < p->length && p->input[p->pos] >= '0' && p->input[p->pos] <= '9';
}

static int gjson__json_number(galloc_Allocator alloc, gjson__Parser *p, double *out) {
    size_t start = p->pos, n, point_length, i, j;
    char *buf, *end;
    const char *point;
    if (p->input[p->pos] == '-')
        ++p->pos;
    if (!gjson__json_digit(p))
        return 0;
    if (p->input[p->pos] == '0')
        ++p->pos;
    else
        while (gjson__json_digit(p))
            ++p->pos;
    if (p->pos < p->length && p->input[p->pos] == '.') {
        ++p->pos;
        if (!gjson__json_digit(p))
            return 0;
        while (gjson__json_digit(p))
            ++p->pos;
    }
    if (p->pos < p->length && (p->input[p->pos] == 'e' || p->input[p->pos] == 'E')) {
        ++p->pos;
        if (p->pos < p->length && (p->input[p->pos] == '+' || p->input[p->pos] == '-'))
            ++p->pos;
        if (!gjson__json_digit(p))
            return 0;
        while (gjson__json_digit(p))
            ++p->pos;
    }
    n = p->pos - start;
    point = localeconv()->decimal_point;
    point_length = strlen(point);
    if (!point_length || n > SIZE_MAX - point_length)
        return 0;
    buf = (char *)galloc_alloc(alloc, n + point_length);
    if (!buf)
        return 0;
    for (i = 0, j = 0; i < n; ++i) {
        if (p->input[start + i] == '.') {
            memcpy(buf + j, point, point_length);
            j += point_length;
        } else
            buf[j++] = (char)p->input[start + i];
    }
    buf[j] = '\0';
    *out = strtod(buf, &end);
    if (end != buf + j) {
        galloc_free(alloc, buf);
        return 0;
    }
    galloc_free(alloc, buf);
    return 1;
}

static gjson_Value *gjson__json_value(galloc_Allocator alloc, gjson__Parser *p, unsigned depth) {
    gjson_Value *v, **tail;
    unsigned char c;
    gjson__json_space(p);
    if (depth > 128 || p->pos >= p->length)
        return NULL;
    v = (gjson_Value *)galloc_calloc(alloc, 1, sizeof(*v));
    if (!v)
        return NULL;
    c = p->input[p->pos];
    if (c == '"') {
        v->type = GJSON_STRING;
        v->as.string.data = gjson__json_string(alloc, p, &v->as.string.length);
        if (!v->as.string.data)
            goto fail;
    } else if (c == '[' || c == '{') {
        unsigned char close = c == '[' ? ']' : '}';
        v->type = c == '[' ? GJSON_ARRAY : GJSON_OBJECT;
        ++p->pos;
        gjson__json_space(p);
        tail = &v->as.children.first;
        if (p->pos < p->length && p->input[p->pos] == close) {
            ++p->pos;
            return v;
        }
        for (;;) {
            char *key = NULL;
            size_t key_len = 0;
            gjson_Value *child;
            if (v->type == GJSON_OBJECT) {
                key = gjson__json_string(alloc, p, &key_len);
                if (!key)
                    goto fail;
                gjson__json_space(p);
                if (p->pos >= p->length || p->input[p->pos++] != ':') {
                    galloc_free(alloc, key);
                    goto fail;
                }
            }
            child = gjson__json_value(alloc, p, depth + 1);
            if (!child) {
                galloc_free(alloc, key);
                goto fail;
            }
            child->key = key;
            child->key_length = key_len;
            *tail = child;
            tail = &child->next;
            ++v->as.children.length;
            gjson__json_space(p);
            if (p->pos >= p->length)
                goto fail;
            c = p->input[p->pos++];
            if (c == close)
                return v;
            if (c != ',')
                goto fail;
            gjson__json_space(p);
        }
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        v->type = GJSON_NUMBER;
        if (!gjson__json_number(alloc, p, &v->as.number))
            goto fail;
    } else if (p->length - p->pos >= 4 && memcmp(p->input + p->pos, "null", 4) == 0) {
        p->pos += 4;
        v->type = GJSON_NULL;
    } else if (p->length - p->pos >= 4 && memcmp(p->input + p->pos, "true", 4) == 0) {
        p->pos += 4;
        v->type = GJSON_BOOL;
        v->as.boolean = 1;
    } else if (p->length - p->pos >= 5 && memcmp(p->input + p->pos, "false", 5) == 0) {
        p->pos += 5;
        v->type = GJSON_BOOL;
    } else
        goto fail;
    return v;
fail:
    gjson_Value_free(alloc, v);
    return NULL;
}

gjson_Value *gjson_parse(galloc_Allocator alloc, const char *input, size_t length, size_t *error_offset) {
    gjson__Parser p;
    gjson_Value *v;
    if (error_offset)
        *error_offset = 0;
    if (!input)
        return NULL;
    p.input = (const unsigned char *)input;
    p.length = length;
    p.pos = 0;
    v = gjson__json_value(alloc, &p, 0);
    gjson__json_space(&p);
    if (!v || p.pos != length) {
        gjson_Value_free(alloc, v);
        if (error_offset)
            *error_offset = p.pos;
        return NULL;
    }
    return v;
}

const gjson_Value *gjson_Value_get_n(const gjson_Value *v, const char *key, size_t length) {
    const gjson_Value *child;
    if (!v || v->type != GJSON_OBJECT || !key)
        return NULL;
    for (child = v->as.children.first; child; child = child->next)
        if (child->key_length == length && memcmp(child->key, key, length) == 0)
            return child;
    return NULL;
}

const gjson_Value *gjson_Value_get(const gjson_Value *v, const char *key) {
    return key ? gjson_Value_get_n(v, key, strlen(key)) : NULL;
}

const gjson_Value *gjson_Value_at(const gjson_Value *v, size_t index) {
    const gjson_Value *child;
    if (!v || v->type != GJSON_ARRAY)
        return NULL;
    child = v->as.children.first;
    while (child && index) {
        child = child->next;
        --index;
    }
    return child;
}

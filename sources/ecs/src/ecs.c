#include "glint/ecs.h"

#include <limits.h>
#include <string.h>

typedef struct {
    char *name;
    size_t size;
    gecs_DestroyFn destroy;
    uint32_t *sparse; /* dense index + 1 */
    size_t sparse_capacity;
    gecs_Entity *entities;
    unsigned char *data;
    size_t count, capacity;
} gecs__Pool;

typedef struct {
    char *name;
    size_t size;
    gecs_DestroyFn destroy;
    void *value;
} gecs__Context;

typedef struct {
    uint32_t generation, next;
    bool alive, destroying;
} gecs__Slot;

struct gecs_World {
    gecs__Slot *slots;
    size_t slots_count, slots_capacity;
    uint32_t free_head;
    gecs__Pool *pools;
    size_t pool_count, pool_capacity;
    gecs__Context *contexts;
    size_t context_count, context_capacity;
    bool frozen;
    bool clearing;
    size_t destroying;
    gecs_EntityPredestroyFn predestroy;
    void *predestroy_user;
};

static bool gecs__grow(galloc_Allocator alloc, void **p, size_t *capacity, size_t need, size_t size) {
    size_t n = *capacity ? *capacity : 8;
    void *q;
    if (need <= *capacity)
        return true;
    while (n < need) {
        if (n > SIZE_MAX / 2)
            return false;
        n *= 2;
    }
    if (size && n > SIZE_MAX / size)
        return false;
    q = galloc_realloc(alloc, *p, *capacity * size, n * size);
    if (!q)
        return false;
    *p = q;
    *capacity = n;
    return true;
}

static char *gecs__copy_string(galloc_Allocator alloc, const char *s) {
    size_t n = strlen(s) + 1;
    char *p = galloc_alloc(alloc, n);
    if (p)
        memcpy(p, s, n);
    return p;
}

static uint32_t gecs__index_of(gecs_Entity e) {
    return (uint32_t)e;
}

static uint32_t gecs__generation_of(gecs_Entity e) {
    return (uint32_t)(e >> 32);
}

static gecs_Entity gecs__make_entity(uint32_t i, uint32_t g) {
    return ((uint64_t)g << 32) | i;
}

static gecs__Pool *gecs__get_pool(const gecs_World *w, gecs_Component c) {
    return w && c && c <= w->pool_count ? &w->pools[c - 1] : NULL;
}

static gecs__Context *gecs__get_context(const gecs_World *w, gecs_Component c) {
    return w && c && c <= w->context_count ? &w->contexts[c - 1] : NULL;
}

gecs_World *gecs_World_new(galloc_Allocator alloc) {
    gecs_World *w = galloc_calloc(alloc, 1, sizeof(*w));
    if (!w)
        return NULL;
    if (!gecs__grow(alloc, (void **)&w->slots, &w->slots_capacity, 1, sizeof(*w->slots))) {
        galloc_free(alloc, w);
        return NULL;
    }
    memset(&w->slots[0], 0, sizeof(w->slots[0]));
    w->slots_count = 1;
    return w;
}

void gecs_World_clear(galloc_Allocator alloc, gecs_World *w) {
    if (!w || w->clearing || w->destroying)
        return;
    w->clearing = true;
    for (size_t i = 1; i < w->slots_count; i++) {
        if (w->slots[i].alive) {
            gecs_Entity entity = gecs__make_entity((uint32_t)i, w->slots[i].generation);
            gecs_Entity_destroy(w, entity);
        }
    }
    for (size_t i = 0; i < w->context_count; i++) {
        gecs_context_remove(alloc, w, (gecs_Component)(i + 1));
    }
    w->clearing = false;
}

void gecs_World_freeze(gecs_World *w) {
    if (w) {
        w->frozen = true;
    }
}

void gecs_World_free(galloc_Allocator alloc, gecs_World *w) {
    if (!w || w->clearing || w->destroying)
        return;
    gecs_World_clear(alloc, w);
    for (size_t i = 0; i < w->pool_count; ++i) {
        gecs__Pool *p = &w->pools[i];
        galloc_free(alloc, p->name);
        galloc_free(alloc, p->sparse);
        galloc_free(alloc, p->entities);
        galloc_free(alloc, p->data);
    }
    for (size_t i = 0; i < w->context_count; ++i) {
        gecs__Context *c = &w->contexts[i];
        galloc_free(alloc, c->name);
    }
    galloc_free(alloc, w->pools);
    galloc_free(alloc, w->contexts);
    galloc_free(alloc, w->slots);
    galloc_free(alloc, w);
}

gecs_Entity gecs_Entity_new(galloc_Allocator alloc, gecs_World *w) {
    uint32_t i;
    gecs__Slot *s;
    if (!w || w->frozen || w->clearing)
        return 0;
    if (w->free_head) {
        i = w->free_head;
        s = &w->slots[i];
        w->free_head = s->next;
    } else {
        if (w->slots_count >= UINT32_MAX
            || !gecs__grow(alloc, (void **)&w->slots, &w->slots_capacity, w->slots_count + 1, sizeof(*w->slots)))
            return 0;
        i = (uint32_t)w->slots_count++;
        s = &w->slots[i];
        memset(s, 0, sizeof(*s));
        s->generation = 1;
    }
    s->alive = true;
    return gecs__make_entity(i, s->generation);
}

bool gecs_Entity_alive(const gecs_World *w, gecs_Entity e) {
    uint32_t i = gecs__index_of(e);
    return w && i && i < w->slots_count && w->slots[i].alive && w->slots[i].generation == gecs__generation_of(e);
}

bool gecs_World_observe_destroy(gecs_World *w, gecs_EntityPredestroyFn callback, void *user) {
    if (!w || !callback || w->predestroy)
        return false;
    w->predestroy = callback;
    w->predestroy_user = user;
    return true;
}

void gecs_World_unobserve_destroy(gecs_World *w, gecs_EntityPredestroyFn callback, void *user) {
    if (w && w->predestroy == callback && w->predestroy_user == user) {
        w->predestroy = NULL;
        w->predestroy_user = NULL;
    }
}

void gecs_Entity_destroy(gecs_World *w, gecs_Entity e) {
    size_t i;
    gecs__Slot *s;
    if (!gecs_Entity_alive(w, e) || w->slots[gecs__index_of(e)].destroying)
        return;
    w->slots[gecs__index_of(e)].destroying = true;
    ++w->destroying;
    if (w->predestroy)
        w->predestroy(w, e, w->predestroy_user);
    for (i = 0; i < w->pool_count; ++i)
        gecs_Component_remove(w, e, (gecs_Component)(i + 1));
    s = &w->slots[gecs__index_of(e)];
    s->alive = false;
    s->destroying = false;
    --w->destroying;
    /* A wrapped generation is retired, never reused. */
    if (++s->generation) {
        s->next = w->free_head;
        w->free_head = gecs__index_of(e);
    }
}

gecs_Component gecs_Component_find(const gecs_World *w, const char *name) {
    size_t i;
    if (!w || !name)
        return 0;
    for (i = 0; i < w->pool_count; ++i)
        if (!strcmp(name, w->pools[i].name))
            return (gecs_Component)(i + 1);
    return 0;
}

gecs_Component
gecs_Component_register(galloc_Allocator alloc, gecs_World *w, const char *name, size_t size, gecs_DestroyFn destroy) {
    gecs_Component id;
    gecs__Pool *p;
    char *copy;
    if (!w || w->frozen || !name || !*name || !size)
        return 0;
    id = gecs_Component_find(w, name);
    if (id) {
        p = gecs__get_pool(w, id);
        return p->size == size && p->destroy == destroy ? id : 0;
    }
    if (w->pool_count == UINT32_MAX)
        return 0;
    copy = gecs__copy_string(alloc, name);
    if (!copy)
        return 0;
    if (!gecs__grow(alloc, (void **)&w->pools, &w->pool_capacity, w->pool_count + 1, sizeof(*w->pools))) {
        galloc_free(alloc, copy);
        return 0;
    }
    p = &w->pools[w->pool_count++];
    memset(p, 0, sizeof(*p));
    p->name = copy;
    p->size = size;
    p->destroy = destroy;
    return (gecs_Component)w->pool_count;
}

void *gecs_Component_get(const gecs_World *w, gecs_Entity e, gecs_Component c) {
    gecs__Pool *p = gecs__get_pool(w, c);
    uint32_t i = gecs__index_of(e), pos;
    if (!p || !gecs_Entity_alive(w, e) || i >= p->sparse_capacity || !(pos = p->sparse[i]))
        return NULL;
    return p->data + (pos - 1) * p->size;
}

void *gecs_Component_set(galloc_Allocator alloc, gecs_World *w, gecs_Entity e, gecs_Component c, const void *value) {
    gecs__Pool *p = gecs__get_pool(w, c);
    uint32_t i = gecs__index_of(e), pos;
    void *out;
    size_t old;
    if (!p || w->frozen || !gecs_Entity_alive(w, e))
        return NULL;
    if (i < p->sparse_capacity && (pos = p->sparse[i])) {
        out = p->data + (pos - 1) * p->size;
        if (value == out)
            return out;
        if (p->destroy && value) {
            /* The source can overlap the old value. Preserve its bytes before
             * the destructor is allowed to modify them. */
            void *copy = galloc_alloc(alloc, p->size);
            if (!copy)
                return NULL;
            memcpy(copy, value, p->size);
            p->destroy(out);
            memcpy(out, copy, p->size);
            galloc_free(alloc, copy);
        } else if (value) {
            memmove(out, value, p->size);
        } else {
            if (p->destroy)
                p->destroy(out);
            memset(out, 0, p->size);
        }
        return out;
    }
    old = p->sparse_capacity;
    if (!gecs__grow(alloc, (void **)&p->sparse, &p->sparse_capacity, (size_t)i + 1, sizeof(*p->sparse)))
        return NULL;
    if (old != p->sparse_capacity)
        memset(p->sparse + old, 0, (p->sparse_capacity - old) * sizeof(*p->sparse));
    if (p->count == UINT32_MAX)
        return NULL;
    if (p->count == p->capacity) {
        size_t cap = p->capacity ? p->capacity * 2 : 8;
        gecs_Entity *entities;
        unsigned char *data;
        if (cap < p->capacity || cap > SIZE_MAX / p->size || cap > SIZE_MAX / sizeof(*entities))
            return NULL;
        /* Allocate both before replacing either; failure leaves the pool intact. */
        entities = galloc_alloc(alloc, cap * sizeof(*entities));
        data = galloc_alloc(alloc, cap * p->size);
        if (!entities || !data) {
            galloc_free(alloc, entities);
            galloc_free(alloc, data);
            return NULL;
        }
        if (p->count) {
            memcpy(entities, p->entities, p->count * sizeof(*entities));
            memcpy(data, p->data, p->count * p->size);
        }
        /* value may point into the existing data; copy it before freeing. */
        out = data + p->count * p->size;
        if (value)
            memmove(out, value, p->size);
        else
            memset(out, 0, p->size);
        galloc_free(alloc, p->entities);
        galloc_free(alloc, p->data);
        p->entities = entities;
        p->data = data;
        p->capacity = cap;
    } else {
        out = p->data + p->count * p->size;
        if (value)
            memmove(out, value, p->size);
        else
            memset(out, 0, p->size);
    }
    p->entities[p->count] = e;
    p->sparse[i] = (uint32_t)++p->count;
    return out;
}

bool gecs_Component_remove(gecs_World *w, gecs_Entity e, gecs_Component c) {
    gecs__Pool *p = gecs__get_pool(w, c);
    uint32_t i = gecs__index_of(e), pos;
    size_t last;
    if (!p || !gecs_Entity_alive(w, e) || i >= p->sparse_capacity || !(pos = p->sparse[i]))
        return false;
    --pos;
    last = --p->count;
    if (p->destroy)
        p->destroy(p->data + pos * p->size);
    if (pos != last) {
        memcpy(p->data + pos * p->size, p->data + last * p->size, p->size);
        p->entities[pos] = p->entities[last];
        p->sparse[gecs__index_of(p->entities[pos])] = pos + 1;
    }
    p->sparse[i] = 0;
    return true;
}

size_t gecs_Component_count(const gecs_World *w, gecs_Component c) {
    gecs__Pool *p = gecs__get_pool(w, c);
    return p ? p->count : 0;
}

gecs_Entity gecs_Component_entity(const gecs_World *w, gecs_Component c, size_t i) {
    gecs__Pool *p = gecs__get_pool(w, c);
    return p && i < p->count ? p->entities[i] : 0;
}

gecs_Component
gecs_context_register(galloc_Allocator alloc, gecs_World *w, const char *name, size_t size, gecs_DestroyFn destroy) {
    size_t i;
    gecs__Context *c;
    char *copy;
    if (!w || w->frozen || !name || !*name || !size)
        return 0;
    for (i = 0; i < w->context_count; ++i)
        if (!strcmp(name, w->contexts[i].name))
            return w->contexts[i].size == size && w->contexts[i].destroy == destroy ? (gecs_Component)(i + 1) : 0;
    if (w->context_count == UINT32_MAX)
        return 0;
    copy = gecs__copy_string(alloc, name);
    if (!copy)
        return 0;
    if (!gecs__grow(alloc, (void **)&w->contexts, &w->context_capacity, w->context_count + 1, sizeof(*w->contexts))) {
        galloc_free(alloc, copy);
        return 0;
    }
    c = &w->contexts[w->context_count++];
    c->name = copy;
    c->size = size;
    c->destroy = destroy;
    c->value = NULL;
    return (gecs_Component)w->context_count;
}

void *gecs_context_set(galloc_Allocator alloc, gecs_World *w, gecs_Component id, const void *value) {
    gecs__Context *c = gecs__get_context(w, id);
    void *p;
    if (!c || w->frozen)
        return NULL;
    if (value == c->value && value)
        return c->value;
    p = galloc_alloc(alloc, c->size);
    if (!p)
        return NULL;
    if (value)
        memcpy(p, value, c->size);
    else
        memset(p, 0, c->size);
    if (c->value && c->destroy)
        c->destroy(c->value);
    galloc_free(alloc, c->value);
    c->value = p;
    return p;
}

void *gecs_context_get(const gecs_World *w, gecs_Component id) {
    gecs__Context *c = gecs__get_context(w, id);
    return c ? c->value : NULL;
}

bool gecs_context_remove(galloc_Allocator alloc, gecs_World *w, gecs_Component id) {
    gecs__Context *c = gecs__get_context(w, id);
    if (!c || !c->value)
        return false;
    if (c->destroy)
        c->destroy(c->value);
    galloc_free(alloc, c->value);
    c->value = NULL;
    return true;
}

#include "./ecs.h"

#include <glint/core/arena.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

typedef struct {
    char* name;
    size_t size;
    gecs_DestroyFn destroy;
    /// Dense index + 1
    uint32_t* sparse;
    size_t sparse_capacity;
    gecs_ID* entities;
    uint8_t* data;
    size_t count;
    size_t capacity;
} gecs__Pool;

typedef struct {
    char* name;
    size_t size;
    gecs_DestroyFn destroy;
    void* value;
} gecs__Context;

typedef struct {
    uint32_t generation;
    uint32_t next;
    bool alive;
    bool destroying;
} gecs__Slot;

struct gecs_World {
    galloc_Allocator alloc;
    garena_Arena temp;

    gecs__Slot* slots;
    size_t slots_count;
    size_t slots_capacity;

    uint32_t free_head;
    gecs__Pool* pools;
    size_t pool_count;
    size_t pool_capacity;

    gecs__Context* contexts;
    size_t context_count;
    size_t context_capacity;

    bool frozen;
    bool clearing;

    size_t destroying;
    gecs_EntityPredestroyFn predestroy;

    void* predestroy_user;
};

static bool gecs__grow(galloc_Allocator alloc, void** p, size_t* capacity, size_t need, size_t size);
static char* gecs__copy_string(galloc_Allocator alloc, const char* s);
static uint32_t gecs__index_of(gecs_ID e);
static uint32_t gecs__generation_of(gecs_ID e);
static gecs_ID gecs__make_entity(uint32_t i, uint32_t g);
static gecs__Pool* gecs__get_pool(const gecs_World* w, gecs_Component c);
static gecs__Context* gecs__get_context(const gecs_World* w, gecs_Component c);

gecs_World* gecs_World_new(galloc_Allocator alloc) {
    gecs_World* w = galloc_calloc(alloc, 1, sizeof(*w));
    if (w == NULL) goto error;

    w->alloc = alloc;
    garena_init(&w->temp, alloc, 0);

    if (!gecs__grow(alloc, (void**)&w->slots, &w->slots_capacity, 1, sizeof(*w->slots))) {
        goto error;
    }

    memset(&w->slots[0], 0, sizeof(w->slots[0]));
    w->slots_count = 1;

    return w;

error:
    galloc_free(alloc, w);
    return NULL;
}

galloc_Allocator glint_allocator(const gecs_World* world) {
    assert(world != NULL);
    return world->alloc;
}

galloc_Allocator glint_temp_allocator(gecs_World* world) {
    assert(world != NULL);
    return garena_allocator(&world->temp);
}

void gecs_World_reset_temp(gecs_World* world) {
    assert(world != NULL);
    garena_reset(&world->temp);
}

void gecs_World_clear_entities(gecs_World* w) {
    assert(w != NULL);

    if (w->clearing || w->destroying > 0) return;

    w->clearing = true;
    for (size_t i = 1; i < w->slots_count; i++) {
        if (w->slots[i].alive) {
            gecs_ID entity = gecs__make_entity((uint32_t)i, w->slots[i].generation);
            gecs_Entity_destroy((gecs_Entity) {w, entity});
        }
    }
    w->clearing = false;
}

void gecs_World_clear_except_many(gecs_World* w, const gecs_Component* contexts, size_t count) {
    assert(w != NULL);

    if (w->clearing || w->destroying > 0 || (count > 0 && contexts == NULL)) return;
    gecs_World_clear_entities(w);

    w->clearing = true;
    for (size_t i = 0; i < w->context_count; i++) {
        bool retained = false;
        for (size_t j = 0; j < count; j++) {
            if ((gecs_Component)(i + 1) == contexts[j]) {
                retained = true;
                break;
            }
        }
        if (!retained) gecs_context_remove(w, (gecs_Component)(i + 1));
    }
    w->clearing = false;
}

void gecs_World_clear_except(gecs_World* w, gecs_Component context) {
    assert(w != NULL);
    gecs_World_clear_except_many(w, &context, 1);
}

void gecs_World_clear(gecs_World* w) {
    assert(w != NULL);
    gecs_World_clear_except_many(w, NULL, 0);
}

void gecs_World_freeze(gecs_World* w) {
    assert(w != NULL);
    w->frozen = true;
}

void gecs_World_free(gecs_World* w) {
    if (w == NULL) return;
    if (w->clearing || w->destroying > 0) return;

    galloc_Allocator alloc = glint_allocator(w);
    gecs_World_clear(w);

    for (size_t i = 0; i < w->pool_count; ++i) {
        gecs__Pool* p = &w->pools[i];
        galloc_free(alloc, p->name);
        galloc_free(alloc, p->sparse);
        galloc_free(alloc, p->entities);
        galloc_free(alloc, p->data);
    }

    for (size_t i = 0; i < w->context_count; ++i) {
        gecs__Context* c = &w->contexts[i];
        galloc_free(alloc, c->name);
    }

    galloc_free(alloc, w->pools);
    galloc_free(alloc, w->contexts);
    galloc_free(alloc, w->slots);
    garena_destroy(&w->temp);
    galloc_free(alloc, w);
}

gecs_Entity gecs_spawn(gecs_World* w) {
    assert(w != NULL);

    galloc_Allocator alloc = glint_allocator(w);
    uint32_t i;
    gecs__Slot* s;

    if (w->frozen || w->clearing) {
        return (gecs_Entity) {0};
    }

    if (w->free_head > 0) {
        i = w->free_head;
        s = &w->slots[i];
        w->free_head = s->next;
    } else {
        if (w->slots_count >= UINT32_MAX
            || !gecs__grow(alloc, (void**)&w->slots, &w->slots_capacity, w->slots_count + 1, sizeof(*w->slots))) {
            return (gecs_Entity) {0};
        }

        i = (uint32_t)w->slots_count++;
        s = &w->slots[i];
        memset(s, 0, sizeof(*s));
        s->generation = 1;
    }

    s->alive = true;

    return (gecs_Entity) {
        .world = w,
        .id = gecs__make_entity(i, s->generation),
    };
}

bool gecs_Entity_alive(gecs_Entity entity) {
    gecs_World* w = entity.world;
    gecs_ID e = entity.id;
    uint32_t i = gecs__index_of(e);
    return w && i && i < w->slots_count && w->slots[i].alive && w->slots[i].generation == gecs__generation_of(e);
}

bool gecs_World_observe_destroy(gecs_World* w, gecs_EntityPredestroyFn callback, void* user) {
    assert(w != NULL);
    if (callback == NULL || w->predestroy != NULL) {
        return false;
    }

    w->predestroy = callback;
    w->predestroy_user = user;

    return true;
}

void gecs_World_unobserve_destroy(gecs_World* w, gecs_EntityPredestroyFn callback, void* user) {
    assert(w != NULL);

    if (w->predestroy == callback && w->predestroy_user == user) {
        w->predestroy = NULL;
        w->predestroy_user = NULL;
    }
}

void gecs_Entity_destroy(gecs_Entity entity) {
    gecs_World* w = entity.world;
    gecs_ID id = entity.id;

    if (!gecs_Entity_alive(entity) || w->slots[gecs__index_of(id)].destroying) {
        return;
    }

    w->slots[gecs__index_of(id)].destroying = true;
    ++w->destroying;

    if (w->predestroy != NULL) {
        w->predestroy(entity, w->predestroy_user);
    }

    for (size_t index = 0; index < w->pool_count; ++index) {
        gecs_Component_remove(entity, (gecs_Component)(index + 1));
    }

    gecs__Slot* sslot = &w->slots[gecs__index_of(id)];
    sslot->alive = false;
    sslot->destroying = false;
    --w->destroying;

    ++sslot->generation;
    if (sslot->generation != 0) {
        sslot->next = w->free_head;
        w->free_head = gecs__index_of(id);
    }
}

gecs_Component gecs_Component_find(const gecs_World* w, const char* name) {
    assert(w != NULL);
    assert(name != NULL);

    if (w == NULL || name == NULL) {
        return 0;
    }

    for (size_t i = 0; i < w->pool_count; ++i) {
        if (strcmp(name, w->pools[i].name) == 0) {
            return (gecs_Component)(i + 1);
        }
    }

    return 0;
}

gecs_Component gecs_Component_register(gecs_World* w, const char* name, size_t size, gecs_DestroyFn destroy) {
    assert(w != NULL);
    assert(name != NULL);

    galloc_Allocator alloc = glint_allocator(w);

    if (w == NULL || name == NULL || *name == 0 || size == 0) {
        return 0;
    }

    gecs_Component id = gecs_Component_find(w, name);

    if (id > 0) {
        gecs__Pool* pool = gecs__get_pool(w, id);
        return pool->size == size && pool->destroy == destroy ? id : 0;
    }

    if (w->frozen) {
        return 0;
    }

    if (w->pool_count == UINT32_MAX) {
        return 0;
    }

    char* copy = gecs__copy_string(alloc, name);
    if (copy == NULL) {
        return 0;
    }

    if (!gecs__grow(alloc, (void**)&w->pools, &w->pool_capacity, w->pool_count + 1, sizeof(*w->pools))) {
        galloc_free(alloc, copy);
        return 0;
    }

    gecs__Pool* pool = &w->pools[w->pool_count++];
    memset(pool, 0, sizeof(*pool));
    pool->name = copy;
    pool->size = size;
    pool->destroy = destroy;

    return (gecs_Component)w->pool_count;
}

void* gecs_Component_get(gecs_Entity entity, gecs_Component c) {
    gecs__Pool* pool = gecs__get_pool(entity.world, c);
    uint32_t index = gecs__index_of(entity.id);

    if (pool == NULL || !gecs_Entity_alive(entity) || index >= pool->sparse_capacity) {
        return NULL;
    }
    uint32_t pos = pool->sparse[index];
    if (pos == 0) {
        return NULL;
    }

    return pool->data + (pos - 1) * pool->size;
}

void* gecs_Component_set(gecs_Entity entity, gecs_Component c, const void* value) {
    gecs_World* w = entity.world;
    galloc_Allocator alloc = glint_allocator(w);

    gecs__Pool* pool = gecs__get_pool(w, c);
    if (pool == NULL || w->frozen || !gecs_Entity_alive(entity)) {
        return NULL;
    }

    uint32_t index = gecs__index_of(entity.id);
    uint32_t pos = index < pool->sparse_capacity ? pool->sparse[index] : 0;
    void* out = NULL;
    if (pos != 0) {
        out = pool->data + (pos - 1) * pool->size;
        if (value == out) {
            return out;
        }

        if (pool->destroy != NULL && value != NULL) {
            void* copy = galloc_alloc(alloc, pool->size);
            if (copy == NULL) {
                return NULL;
            }

            memcpy(copy, value, pool->size);
            pool->destroy(out);
            memcpy(out, copy, pool->size);

            galloc_free(alloc, copy);
        } else if (value != NULL) {
            memmove(out, value, pool->size);
        } else {
            if (pool->destroy != NULL) {
                pool->destroy(out);
            }
            memset(out, 0, pool->size);
        }

        return out;
    }

    size_t old = pool->sparse_capacity;
    if (!gecs__grow(alloc, (void**)&pool->sparse, &pool->sparse_capacity, (size_t)index + 1, sizeof(*pool->sparse))) {
        return NULL;
    }

    if (old != pool->sparse_capacity) {
        memset(pool->sparse + old, 0, (pool->sparse_capacity - old) * sizeof(*pool->sparse));
    }

    if (pool->count == UINT32_MAX) {
        return NULL;
    }

    if (pool->count == pool->capacity) {
        size_t cap = pool->capacity != 0 ? pool->capacity * 2 : 8;
        gecs_ID* entities;
        uint8_t* data;
        if (cap < pool->capacity || cap > SIZE_MAX / pool->size || cap > SIZE_MAX / sizeof(*entities)) {
            return NULL;
        }

        entities = galloc_alloc(alloc, cap * sizeof(*entities));
        data = galloc_alloc(alloc, cap * pool->size);
        if (entities == NULL || data == NULL) {
            galloc_free(alloc, entities);
            galloc_free(alloc, data);
            return NULL;
        }

        if (pool->count > 0) {
            memcpy(entities, pool->entities, pool->count * sizeof(*entities));
            memcpy(data, pool->data, pool->count * pool->size);
        }

        out = data + pool->count * pool->size;
        if (value != NULL) {
            memmove(out, value, pool->size);
        } else {
            memset(out, 0, pool->size);
        }

        galloc_free(alloc, pool->entities);
        galloc_free(alloc, pool->data);

        pool->entities = entities;
        pool->data = data;
        pool->capacity = cap;
    } else {
        out = pool->data + pool->count * pool->size;
        if (value != NULL) {
            memmove(out, value, pool->size);
        } else {
            memset(out, 0, pool->size);
        }
    }

    pool->entities[pool->count] = entity.id;
    pool->sparse[index] = (uint32_t)++pool->count;

    return out;
}

bool gecs_Component_remove(gecs_Entity entity, gecs_Component c) {
    gecs_World* w = entity.world;
    gecs__Pool* pool = gecs__get_pool(w, c);
    uint32_t index = gecs__index_of(entity.id);

    bool pool_exists = pool != NULL;
    bool entity_alive = gecs_Entity_alive(entity);
    if (!pool_exists || !entity_alive) {
        return false;
    }

    bool index_in_bounds = index < pool->sparse_capacity;
    if (!index_in_bounds) {
        return false;
    }

    size_t pos = pool->sparse[index];
    bool has_component = pos != 0;
    if (!has_component) {
        return false;
    }

    pos -= 1;
    size_t last = --pool->count;
    if (pool->destroy != NULL) {
        pool->destroy(pool->data + pos * pool->size);
    }

    if (pos != last) {
        memcpy(pool->data + pos * pool->size, pool->data + last * pool->size, pool->size);
        pool->entities[pos] = pool->entities[last];
        pool->sparse[gecs__index_of(pool->entities[pos])] = pos + 1;
    }

    pool->sparse[index] = 0;

    return true;
}

size_t gecs_Component_count(const gecs_World* w, gecs_Component c) {
    gecs__Pool* p = gecs__get_pool(w, c);
    return p != NULL ? p->count : 0;
}

gecs_Entity gecs_Component_entity(gecs_World* w, gecs_Component c, size_t i) {
    gecs__Pool* p = gecs__get_pool(w, c);
    return p != NULL && i < p->count ? (gecs_Entity) {.world = w, .id = p->entities[i]} : (gecs_Entity) {0};
}

gecs_Component gecs_context_find(const gecs_World* w, const char* name) {
    assert(w != NULL);
    assert(name != NULL);

    if (w == NULL || name == NULL) {
        return 0;
    }

    for (size_t i = 0; i < w->context_count; i++) {
        if (strcmp(name, w->contexts[i].name) == 0) {
            return (gecs_Component)(i + 1);
        }
    }

    return 0;
}

gecs_Component gecs_context_register(gecs_World* w, const char* name, size_t size, gecs_DestroyFn destroy) {
    assert(w != NULL);
    assert(name != NULL);

    if (w == NULL || name == NULL || *name == 0 || size == 0) {
        return 0;
    }

    for (size_t i = 0; i < w->context_count; ++i) {
        if (strcmp(name, w->contexts[i].name) == 0) {
            return w->contexts[i].size == size && w->contexts[i].destroy == destroy ? (gecs_Component)(i + 1) : 0;
        }
    }

    if (w->frozen) {
        return 0;
    }

    if (w->context_count == UINT32_MAX) {
        return 0;
    }

    galloc_Allocator alloc = glint_allocator(w);
    char* copy = gecs__copy_string(alloc, name);
    if (copy == NULL) return 0;
    if (!gecs__grow(alloc, (void**)&w->contexts, &w->context_capacity, w->context_count + 1, sizeof(*w->contexts))) {
        galloc_free(alloc, copy);
        return 0;
    }

    gecs__Context* c = &w->contexts[w->context_count++];

    c->name = copy;
    c->size = size;
    c->destroy = destroy;
    c->value = NULL;

    return (gecs_Component)w->context_count;
}

void* gecs_context_set(gecs_World* w, gecs_Component id, const void* value) {
    assert(w != NULL);

    gecs__Context* c = gecs__get_context(w, id);
    if (c == NULL || w->frozen) {
        return NULL;
    }

    if (value == c->value && value != NULL) {
        return c->value;
    }

    galloc_Allocator alloc = glint_allocator(w);
    void* p = galloc_alloc(alloc, c->size);
    if (p == NULL) {
        return NULL;
    }

    if (value != NULL) {
        memcpy(p, value, c->size);
    } else {
        memset(p, 0, c->size);
    }

    if (c->value != NULL && c->destroy != NULL) {
        c->destroy(c->value);
    }

    galloc_free(alloc, c->value);
    c->value = p;

    return p;
}

void* gecs_context_get(const gecs_World* w, gecs_Component id) {
    assert(w != NULL);
    gecs__Context* c = gecs__get_context(w, id);
    return c != NULL ? c->value : NULL;
}

bool gecs_context_remove(gecs_World* w, gecs_Component id) {
    galloc_Allocator alloc = glint_allocator(w);
    gecs__Context* c = gecs__get_context(w, id);

    if (c == NULL || c->value == NULL) {
        return false;
    }

    if (c->destroy != NULL) {
        c->destroy(c->value);
    }

    galloc_free(alloc, c->value);
    c->value = NULL;

    return true;
}

static bool gecs__grow(galloc_Allocator alloc, void** p, size_t* capacity, size_t need, size_t size) {
    if (need <= *capacity) {
        return true;
    }

    size_t new_capacity = *capacity != 0 ? *capacity : 8;
    while (new_capacity < need) {
        if (new_capacity > SIZE_MAX / 2) {
            return false;
        }

        new_capacity *= 2;
    }

    if (size > 0 && new_capacity > SIZE_MAX / size) {
        return false;
    }

    void* q = galloc_realloc(alloc, *p, *capacity * size, new_capacity * size);
    if (q == NULL) return false;
    *p = q;
    *capacity = new_capacity;

    return true;
}

static char* gecs__copy_string(galloc_Allocator alloc, const char* s) {
    size_t n = strlen(s) + 1;
    char* p = galloc_alloc(alloc, n);
    if (p != NULL) {
        memcpy(p, s, n);
    }

    return p;
}

static uint32_t gecs__index_of(gecs_ID e) {
    return (uint32_t)e;
}

static uint32_t gecs__generation_of(gecs_ID e) {
    return (uint32_t)(e >> 32);
}

static gecs_ID gecs__make_entity(uint32_t i, uint32_t g) {
    return ((uint64_t)g << 32) | i;
}

static gecs__Pool* gecs__get_pool(const gecs_World* w, gecs_Component c) {
    assert(w != NULL);
    return w != NULL && c != 0 && c <= w->pool_count ? &w->pools[c - 1] : NULL;
}

static gecs__Context* gecs__get_context(const gecs_World* w, gecs_Component c) {
    assert(w != NULL);
    return w != NULL && c != 0 && c <= w->context_count ? &w->contexts[c - 1] : NULL;
}

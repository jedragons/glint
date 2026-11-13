#include "glint/ecs.h"
#include <mewunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int x, y;
} gecs__TestPosition;

typedef struct {
    galloc_Allocator alloc;
    int *ptr;
} gecs__TestOwned;

static int gecs__test_destroyed;

static void gecs__test_destroy_owned(void *p) {
    gecs__TestOwned *v = p;
    galloc_free(v->alloc, v->ptr);
    ++gecs__test_destroyed;
}

static gecs__TestOwned gecs__test_owned(galloc_Allocator alloc, int v) {
    gecs__TestOwned o;
    o.alloc = alloc;
    o.ptr = galloc_alloc(alloc, sizeof(int));
    mewassert("Test expectation failed", o.ptr);
    *o.ptr = v;
    return o;
}

typedef struct gecs__TestAllocator {
    galloc_Allocator backing;
    bool fail;
    size_t live;
} gecs__TestAllocator;

static void *gecs__test_test_alloc(void *data, size_t size) {
    gecs__TestAllocator *state = data;
    if (state->fail)
        return NULL;
    void *p = galloc_alloc(state->backing, size);
    if (p)
        state->live++;
    return p;
}

static void gecs__test_test_free(void *data, void *ptr) {
    gecs__TestAllocator *state = data;
    mewrequire("Test expectation failed", state->live);
    state->live--;
    galloc_free(state->backing, ptr);
}

static void *gecs__test_test_calloc(void *data, size_t count, size_t size) {
    gecs__TestAllocator *state = data;
    if (state->fail)
        return NULL;
    void *p = galloc_calloc(state->backing, count, size);
    if (p)
        state->live++;
    return p;
}

static void *gecs__test_test_realloc(void *data, void *ptr, size_t old_size, size_t new_size) {
    gecs__TestAllocator *state = data;
    if (state->fail)
        return NULL;
    void *result = galloc_realloc(state->backing, ptr, old_size, new_size);
    if (result && !ptr)
        state->live++;
    return result;
}

typedef struct gecs__TestBytes {
    unsigned char data[8];
} gecs__TestBytes;

static void gecs__test_clear_bytes(void *value) {
    memset(value, 0, sizeof(gecs__TestBytes));
}

static void gecs__test_test_aliasing_and_failures(void) {
    static const galloc_FunctionTable gecs__test_table = {gecs__test_test_alloc, gecs__test_test_free, gecs__test_test_calloc, gecs__test_test_realloc};
    gecs__TestAllocator state = {.backing = galloc_malloc_allocator()};
    galloc_Allocator alloc = {.data = &state, .ftable = &gecs__test_table};
    state.fail = true;
    mewassert("Test expectation failed", !gecs_World_new(alloc));
    mewassert("Test expectation failed", state.live == 0);
    state.fail = false;
    gecs_World *world = gecs_World_new(alloc);
    mewassert("Test expectation failed", world);
    state.fail = true;
    mewassert("Test expectation failed", !gecs_Component_register(alloc, world, "fail", sizeof(gecs__TestBytes), NULL));
    mewassert("Test expectation failed", !gecs_Component_find(world, "fail"));
    state.fail = false;
    gecs_Component component = gecs_Component_register(alloc, world, "Bytes", sizeof(gecs__TestBytes), gecs__test_clear_bytes);
    mewassert("Test expectation failed", component);
    gecs_Entity entities[9];
    for (size_t i = 0; i < 9; i++) {
        entities[i] = gecs_Entity_new(alloc, world);
        mewassert("Test expectation failed", entities[i]);
    }
    gecs__TestBytes first = {{0, 1, 2, 3, 4, 5, 6, 7}};
    gecs__TestBytes second = {{8, 9, 10, 11, 12, 13, 14, 15}};
    mewassert("Test expectation failed", gecs_Component_set(alloc, world, entities[0], component, &first));
    mewassert("Test expectation failed", gecs_Component_set(alloc, world, entities[1], component, &second));
    gecs__TestBytes *stored = gecs_Component_get(world, entities[0], component);
    mewassert("Test expectation failed", gecs_Component_set(alloc, world, entities[0], component, stored) == stored);
    state.fail = true;
    mewassert("Test expectation failed", !gecs_Component_set(alloc, world, entities[0], component, &second));
    mewassert("Test expectation failed", !memcmp(stored, &first, sizeof(first)));
    state.fail = false;
    /* The source overlaps the destination, so the destructor must not alter it
     * before the copy has been made. */
    mewassert("Test expectation failed", gecs_Component_set(alloc, world, entities[0], component, stored->data + 1));
    const unsigned char expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    mewassert("Test expectation failed", !memcmp(gecs_Component_get(world, entities[0], component), expected, sizeof(expected)));
    for (size_t i = 2; i < 8; i++) {
        mewassert("Test expectation failed", gecs_Component_set(alloc, world, entities[i], component, &second));
    }
    /* The ninth insertion grows the pool while its input aliases that pool. */
    stored = gecs_Component_get(world, entities[0], component);
    state.fail = true;
    mewassert("Test expectation failed", !gecs_Component_set(alloc, world, entities[8], component, stored));
    mewassert("Test expectation failed", gecs_Component_count(world, component) == 8);
    mewassert("Test expectation failed", !memcmp(gecs_Component_get(world, entities[0], component), expected, sizeof(expected)));
    state.fail = false;
    mewassert("Test expectation failed", gecs_Component_set(alloc, world, entities[8], component, stored));
    mewassert("Test expectation failed", !memcmp(gecs_Component_get(world, entities[8], component), expected, sizeof(expected)));
    gecs_Component context = gecs_context_register(alloc, world, "Bytes", sizeof(gecs__TestBytes), gecs__test_clear_bytes);
    mewassert("Test expectation failed", context);
    gecs__TestBytes *ctx = gecs_context_set(alloc, world, context, &first);
    mewassert("Test expectation failed", ctx);
    mewassert("Test expectation failed", gecs_context_set(alloc, world, context, ctx) == ctx);
    state.fail = true;
    mewassert("Test expectation failed", !gecs_context_set(alloc, world, context, &second));
    mewassert("Test expectation failed", !memcmp(gecs_context_get(world, context), &first, sizeof(first)));
    state.fail = false;
    mewassert("Test expectation failed", gecs_context_remove(alloc, world, context));
    size_t index = 0;
    while (index < gecs_Component_count(world, component)) {
        gecs_Entity entity = gecs_Component_entity(world, component, index);
        mewassert("Test expectation failed", entity && gecs_Component_remove(world, entity, component));
    }
    mewassert("Test expectation failed", !gecs_Component_count(world, component));
    mewassert("Test expectation failed", !gecs_Component_entity(world, component, 0));
    gecs_World_free(alloc, world);
    mewassert("Test expectation failed", state.live == 0);
}

TEST(ecs_test, {
    gecs__test_test_aliasing_and_failures();
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *w = gecs_World_new(alloc);
    gecs_Component pos, own, ctx;
    gecs_Entity e[20], stale, replacement;
    size_t i;
    gecs__TestPosition p = {3, 4};
    gecs__TestOwned o;
    mewassert("Test expectation failed", w);
    pos = GECS_COMPONENT(alloc, w, gecs__TestPosition);
    own = gecs_Component_register(alloc, w, "Owned", sizeof(gecs__TestOwned), gecs__test_destroy_owned);
    ctx = gecs_context_register(alloc, w, "Owned", sizeof(gecs__TestOwned), gecs__test_destroy_owned);
    mewassert("Test expectation failed", pos && own && ctx && GECS_COMPONENT(alloc, w, gecs__TestPosition) == pos);
    mewassert("Test expectation failed", !gecs_Component_register(alloc, w, "gecs__TestPosition", 1, NULL));
    mewassert("Test expectation failed", gecs_Component_find(w, "gecs__TestPosition") == pos);
    for (i = 0; i < 20; ++i) {
        e[i] = gecs_Entity_new(alloc, w);
        mewassert("Test expectation failed", e[i]);
        mewassert("Test expectation failed", gecs_Component_set(alloc, w, e[i], pos, &p));
    }
    mewassert("Test expectation failed", gecs_Component_count(w, pos) == 20);
    mewassert("Test expectation failed", gecs_Component_remove(w, e[4], pos));
    mewassert("Test expectation failed", !gecs_Component_get(w, e[4], pos));
    mewassert("Test expectation failed", ((gecs__TestPosition *)gecs_Component_get(w, e[19], pos))->y == 4);
    mewassert("Test expectation failed", gecs_Component_entity(w, pos, 4) == e[19]);
    o = gecs__test_owned(alloc, 1);
    mewassert("Test expectation failed", gecs_Component_set(alloc, w, e[5], own, &o));
    o = gecs__test_owned(alloc, 2);
    mewassert("Test expectation failed", gecs_Component_set(alloc, w, e[5], own, &o));
    mewassert("Test expectation failed", gecs__test_destroyed == 1);
    stale = e[5];
    gecs_Entity_destroy(w, stale);
    mewassert("Test expectation failed", gecs__test_destroyed == 2);
    replacement = gecs_Entity_new(alloc, w);
    mewassert("Test expectation failed", replacement && replacement != stale);
    mewassert("Test expectation failed", !gecs_Entity_alive(w, stale) && !gecs_Component_set(alloc, w, stale, pos, &p));
    mewassert("Test expectation failed", gecs_Entity_alive(w, replacement));
    o = gecs__test_owned(alloc, 3);
    mewassert("Test expectation failed", gecs_context_set(alloc, w, ctx, &o));
    mewassert("Test expectation failed", *((gecs__TestOwned *)gecs_context_get(w, ctx))->ptr == 3);
    o = gecs__test_owned(alloc, 4);
    mewassert("Test expectation failed", gecs_context_set(alloc, w, ctx, &o));
    mewassert("Test expectation failed", gecs__test_destroyed == 3);
    mewassert("Test expectation failed", gecs_context_remove(alloc, w, ctx));
    mewassert("Test expectation failed", gecs__test_destroyed == 4);
    o = gecs__test_owned(alloc, 5);
    mewassert("Test expectation failed", gecs_Component_set(alloc, w, replacement, own, &o));
    o = gecs__test_owned(alloc, 6);
    mewassert("Test expectation failed", gecs_context_set(alloc, w, ctx, &o));
    gecs_World_free(alloc, w);
    mewassert("Test expectation failed", gecs__test_destroyed == 6);
    puts("ecs tests passed");
})

static int gecs__test_clear_destroyed;

static void gecs__test_clear_destructor(void *value) {
    (void)value;
    gecs__test_clear_destroyed++;
}

TEST(clear_and_freeze, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    mewassert("world created", world);
    gecs_Component component = gecs_Component_register(alloc, world, "ClearComponent", sizeof(int), gecs__test_clear_destructor);
    gecs_Component context = gecs_context_register(alloc, world, "ClearContext", sizeof(int), gecs__test_clear_destructor);
    mewassert("types registered", component && context);
    gecs_Entity first = gecs_Entity_new(alloc, world);
    gecs_Entity second = gecs_Entity_new(alloc, world);
    int value = 42;
    mewassert("component created", gecs_Component_set(alloc, world, first, component, &value));
    mewassert("component created", gecs_Component_set(alloc, world, second, component, &value));
    mewassert("context created", gecs_context_set(alloc, world, context, &value));
    gecs__test_clear_destroyed = 0;
    gecs_World_clear(alloc, world);
    mewassert("all values destroyed", gecs__test_clear_destroyed == 3);
    mewassert("old handles invalid", !gecs_Entity_alive(world, first) && !gecs_Entity_alive(world, second));
    mewassert("component storage empty", gecs_Component_count(world, component) == 0);
    mewassert("context empty", !gecs_context_get(world, context));
    gecs_World_clear(alloc, world);
    mewassert("clear is idempotent", gecs__test_clear_destroyed == 3);
    mewassert("registration retained", gecs_Component_find(world, "ClearComponent") == component);
    mewassert("component registered unchanged", gecs_Component_register(alloc, world, "ClearComponent", sizeof(int), gecs__test_clear_destructor) == component);
    gecs_Entity replacement = gecs_Entity_new(alloc, world);
    mewassert("recycled entity has fresh generation", replacement && replacement != first && replacement != second);
    mewassert("component works after clear", gecs_Component_set(alloc, world, replacement, component, &value));
    gecs_World_freeze(world);
    mewassert("freeze rejects entity creation", !gecs_Entity_new(alloc, world));
    mewassert("freeze rejects registration", !gecs_Component_register(alloc, world, "Other", sizeof(int), NULL));
    mewassert("freeze rejects context registration", !gecs_context_register(alloc, world, "Other", sizeof(int), NULL));
    mewassert("freeze rejects setting components", !gecs_Component_set(alloc, world, replacement, component, &value));
    mewassert("freeze rejects setting contexts", !gecs_context_set(alloc, world, context, &value));
    mewassert("existing value remains readable", gecs_Component_get(world, replacement, component));
    gecs_World_clear(alloc, world);
    mewassert("clear still works when frozen", gecs__test_clear_destroyed == 4 && !gecs_Entity_alive(world, replacement));
    gecs_World_free(alloc, world);
    mewassert("free does not destroy twice", gecs__test_clear_destroyed == 4);
})

typedef struct gecs__TestObserverTest {
    galloc_Allocator alloc;
    gecs_Component component;
    gecs_Entity other;
    int calls;
} gecs__TestObserverTest;

static void gecs__test_observe_destroy(gecs_World *world, gecs_Entity entity, void *user) {
    gecs__TestObserverTest *test = user;
    mewassert("entity and component remain available", gecs_Entity_alive(world, entity)
              && gecs_Component_get(world, entity, test->component));
    test->calls++;
    gecs_Entity_destroy(world, entity);
    gecs_World_clear(test->alloc, world);
    gecs_World_free(test->alloc, world);
    if (test->other && test->other != entity) {
        gecs_Entity other = test->other;
        test->other = 0;
        gecs_Entity_destroy(world, other);
    }
    mewassert("recursive teardown ignored", gecs_Entity_alive(world, entity));
}

TEST(predestroy_observer, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gecs__TestObserverTest test = {.alloc = alloc};
    test.component = GECS_COMPONENT(alloc, world, int);
    gecs_Entity first = gecs_Entity_new(alloc, world);
    test.other = gecs_Entity_new(alloc, world);
    mewassert("components", gecs_Component_set(alloc, world, first, test.component, NULL)
              && gecs_Component_set(alloc, world, test.other, test.component, NULL));
    mewassert("observer", gecs_World_observe_destroy(world, gecs__test_observe_destroy, &test));
    mewassert("exclusive", !gecs_World_observe_destroy(world, gecs__test_observe_destroy, &test));
    gecs_Entity_destroy(world, first);
    mewassert("nested destroys exactly once", test.calls == 2 && !gecs_Entity_alive(world, first));
    gecs_World_unobserve_destroy(world, gecs__test_observe_destroy, &test);
    gecs_World_free(alloc, world);
})

mewtest_main();

#include <glint/ecs/ecs.h>
#include <glint/unit/unit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t x;
    int32_t y;
} gecs__TestPosition;

typedef struct {
    galloc_Allocator alloc;
    int32_t* ptr;
} gecs__TestOwned;

static size_t gecs__test_destroyed;
static size_t gecs__test_clear_destroyed;

typedef struct gecs__TestAllocator {
    galloc_Allocator backing;
    bool fail;
    size_t live;
} gecs__TestAllocator;

typedef struct gecs__TestBytes {
    uint8_t data[8];
} gecs__TestBytes;

typedef struct gecs__TestObserver {
    gecs_Entity last_entity;
    size_t calls;
} gecs__TestObserver;

enum { GECS_TEST_ENTITY_CAPACITY = 20 };

static void gecs__test_destroy_owned(void* p);
static void* gecs__test_alloc(void* data, size_t size);
static void gecs__test_free(void* data, void* ptr);
static void* gecs__test_calloc(void* data, size_t count, size_t size);
static void* gecs__test_realloc(void* data, void* ptr, size_t old_size, size_t new_size);
static void gecs__test_clear_bytes(void* value);
static void gecs__test_clear_destructor(void* value);
static void gecs__test_observe_destroy(gecs_Entity handle, void* user);

typedef struct gecs__TestContext {
    gecs__TestAllocator allocator;
    galloc_Allocator alloc;
    gecs_World* world;
    gecs_World* second_world;
    gecs_Component position;
    gecs_Component owned;
    gecs_Component context;
    gecs_Component bytes;
    gecs_Component preserved_contexts[2];
    gecs_Entity entities[GECS_TEST_ENTITY_CAPACITY];
    gecs__TestObserver observer;
    int32_t* pending_owned;
} gecs__TestContext;

static void gecs__test_context_allocator(gecs__TestContext* context);
static bool gecs__test_context_create(gecs__TestContext* context);
static void gecs__test_context_free(void* user);
static bool gecs__test_positions_create(gecs__TestContext* context, size_t count);
static bool gecs__test_owned_create(gecs__TestContext* context);
static bool gecs__test_put_owned(gecs__TestContext* context, gecs_Entity entity, bool in_context, int32_t value);
static bool gecs__test_clear_create(gecs__TestContext* context);
static bool gecs__test_bytes_create(gecs__TestContext* context);
static bool gecs__test_staged_clear_create(gecs__TestContext* context);

GUNIT_TEST(entity_handles_are_world_bound,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    context.second_world = gecs_World_new(context.alloc);
    gunit_assert("second world created", context.second_world != NULL);
    gecs_Entity first = gecs_spawn(context.world);
    gecs_Entity second = gecs_spawn(context.second_world);
    gunit_assert("entities created", first.id != 0 && second.id != 0);
    context.position = GECS_COMPONENT(context.world, gecs__TestPosition);
    gecs_Component second_position = GECS_COMPONENT(context.second_world, gecs__TestPosition);
    gunit_assert("positions registered in both worlds", context.position != 0 && second_position != 0);
    gecs__TestPosition first_value = {3, 4};
    gecs__TestPosition second_value = {7, 8};
    gunit_assert("first world stores value", gecs_Component_set(first, context.position, &first_value) != NULL);
    gunit_assert("second world stores value", gecs_Component_set(second, second_position, &second_value) != NULL);
    const gecs__TestPosition* stored_first = gecs_Component_get(first, context.position);
    const gecs__TestPosition* stored_second = gecs_Component_get(second, second_position);
    gunit_assert("first world remains isolated", stored_first != NULL && stored_first->x == 3 && stored_first->y == 4);
    gunit_assert(
        "second world remains isolated",
        stored_second != NULL && stored_second->x == 7 && stored_second->y == 8
    );
    gecs_Entity found = gecs_Component_entity(context.world, context.position, 0);
    gunit_assert("iteration returns world-bound handle", found.world == context.world && found.id == first.id);
)

GUNIT_TEST(destroyed_handles_remain_stale,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    context.second_world = gecs_World_new(context.alloc);
    gunit_assert("second world created", context.second_world != NULL);
    gecs_Entity first = gecs_spawn(context.world);
    gecs_Entity other = gecs_spawn(context.second_world);
    gecs_Entity_destroy(first);
    gecs_Entity replacement = gecs_spawn(context.world);
    gunit_assert("destroyed handle remains stale", !gecs_Entity_alive(first));
    gunit_assert("replacement has new generation", replacement.id != 0 && replacement.id != first.id);
    gunit_assert("other world entity remains alive", gecs_Entity_alive(other));
    gunit_assert("zero handle is invalid", !gecs_Entity_alive((gecs_Entity) {0}));
)

GUNIT_TEST(component_registration_is_consistent,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("register owned value fixtures", gecs__test_owned_create(&context));
    context.position = GECS_COMPONENT(context.world, gecs__TestPosition);
    gunit_assert(
        "component and context registered",
        context.position != 0 && context.owned != 0 && context.context != 0
    );
    gunit_assert(
        "repeated registration retains type",
        GECS_COMPONENT(context.world, gecs__TestPosition) == context.position
    );
    gunit_assert(
        "incompatible registration rejected",
        gecs_Component_register(context.world, "gecs__TestPosition", 1, NULL) == 0
    );
    gunit_assert(
        "find registered component",
        gecs_Component_find(context.world, "gecs__TestPosition") == context.position
    );
)

GUNIT_TEST(component_removal_preserves_remaining_entities,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    const size_t count = GECS_TEST_ENTITY_CAPACITY;
    gunit_assert("create position fixtures", gecs__test_positions_create(&context, count));
    const size_t removed = 4;
    gunit_assert("all component values stored", gecs_Component_count(context.world, context.position) == count);

    gunit_assert("remove selected position", gecs_Component_remove(context.entities[removed], context.position));
    gunit_assert("removed position absent", gecs_Component_get(context.entities[removed], context.position) == NULL);
    gunit_assert("only selected component removed", gecs_Component_count(context.world, context.position) == count - 1);

    bool seen[GECS_TEST_ENTITY_CAPACITY] = {0};
    for (size_t index = 0; index < gecs_Component_count(context.world, context.position); index++) {
        gecs_Entity entity = gecs_Component_entity(context.world, context.position, index);
        size_t original = 0;
        while (original < count && context.entities[original].id != entity.id) {
            original += 1;
        }
        gunit_assert(
            "iteration contains only surviving original entities",
            entity.world == context.world && original < count && original != removed
        );
        gunit_assert("iteration contains no duplicate entities", !seen[original]);
        seen[original] = true;
    }

    for (size_t i = 0; i < count; i++) {
        if (i == removed) {
            continue;
        }
        const gecs__TestPosition* position = gecs_Component_get(context.entities[i], context.position);
        gunit_assert("surviving entity remains alive", gecs_Entity_alive(context.entities[i]));
        gunit_assert(
            "surviving component retains value",
            position != NULL && position->x == (int32_t)i + 3 && position->y == 4
        );
        gunit_assert("each survivor is iterable", seen[i]);
    }
)

GUNIT_TEST(component_replacement_destroys_previous_value,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("register owned value fixtures", gecs__test_owned_create(&context));
    gecs_Entity entity = gecs_spawn(context.world);
    gunit_assert("store original owned component", gecs__test_put_owned(&context, entity, false, 1));

    gunit_assert("replace owned component", gecs__test_put_owned(&context, entity, false, 2));
    gunit_assert("replacement destroys old value", gecs__test_destroyed == 1);

    gecs_Entity_destroy(entity);
    gunit_assert("entity destruction frees owned value", gecs__test_destroyed == 2);
)

GUNIT_TEST(recycled_entities_reject_stale_component_writes,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create position fixtures", gecs__test_positions_create(&context, 1));
    gecs_Entity stale = context.entities[0];
    gecs_Entity_destroy(stale);
    gecs_Entity replacement = gecs_spawn(context.world);
    gecs__TestPosition position = {3, 4};
    gunit_assert("recycled slot has new generation", replacement.id != 0 && replacement.id != stale.id);
    gunit_assert(
        "stale entity rejected",
        !gecs_Entity_alive(stale) && gecs_Component_set(stale, context.position, &position) == NULL
    );
    gunit_assert("replacement entity alive", gecs_Entity_alive(replacement));
)

GUNIT_TEST(context_replacement_and_removal_destroy_values,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("register owned value fixtures", gecs__test_owned_create(&context));
    gunit_assert("store original owned context", gecs__test_put_owned(&context, (gecs_Entity) {0}, true, 3));
    const gecs__TestOwned* owned = gecs_context_get(context.world, context.context);
    gunit_assert("context retains value", owned != NULL && owned->ptr != NULL && *owned->ptr == 3);

    gunit_assert("replace owned context", gecs__test_put_owned(&context, (gecs_Entity) {0}, true, 4));
    gunit_assert("replacement destroys old context value", gecs__test_destroyed == 1);

    gunit_assert("remove context", gecs_context_remove(context.world, context.context));
    gunit_assert("removal destroys context value", gecs__test_destroyed == 2);
)

GUNIT_TEST(world_free_destroys_components_and_contexts,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("register owned value fixtures", gecs__test_owned_create(&context));
    gunit_assert("store owned fixture value", gecs__test_put_owned(&context, gecs_spawn(context.world), false, 5));
    gunit_assert("store owned fixture value", gecs__test_put_owned(&context, (gecs_Entity) {0}, true, 6));
    gecs__test_context_free(&context);
    gunit_assert("world teardown destroys both values", gecs__test_destroyed == 2);
    gunit_assert("world teardown releases allocations", context.allocator.live == 0);
)

GUNIT_TEST(world_clear_destroys_values_and_invalidates_handles,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create clear fixtures", gecs__test_clear_create(&context));
    gecs_World_clear(context.world);
    gunit_assert("all values destroyed", gecs__test_clear_destroyed == 3);
    gunit_assert(
        "old handles invalid",
        !gecs_Entity_alive(context.entities[0]) && !gecs_Entity_alive(context.entities[1])
    );
    gunit_assert("component storage empty", gecs_Component_count(context.world, context.position) == 0);
    gunit_assert("context empty", gecs_context_get(context.world, context.context) == NULL);
    gecs_World_clear(context.world);
    gunit_assert("clear is idempotent", gecs__test_clear_destroyed == 3);
)

GUNIT_TEST(world_clear_retains_registration,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create clear fixtures", gecs__test_clear_create(&context));
    gecs_World_clear(context.world);
    gunit_assert("registration retained", gecs_Component_find(context.world, "ClearComponent") == context.position);
    gunit_assert(
        "registration unchanged",
        gecs_Component_register(context.world, "ClearComponent", sizeof(int32_t), gecs__test_clear_destructor)
            == context.position
    );
    gecs_Entity replacement = gecs_spawn(context.world);
    gunit_assert(
        "new entity has fresh generation",
        replacement.id != 0 && replacement.id != context.entities[0].id && replacement.id != context.entities[1].id
    );
    int32_t value = 42;
    gunit_assert("component works after clear", gecs_Component_set(replacement, context.position, &value) != NULL);
)

GUNIT_TEST(frozen_world_resolves_existing_types,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create clear fixtures", gecs__test_clear_create(&context));
    gecs_World_freeze(context.world);
    gunit_assert(
        "existing component resolves",
        gecs_Component_register(context.world, "ClearComponent", sizeof(int32_t), gecs__test_clear_destructor)
            == context.position
    );
    gunit_assert(
        "existing context resolves",
        gecs_context_register(context.world, "ClearContext", sizeof(int32_t), gecs__test_clear_destructor)
            == context.context
    );
    gunit_assert(
        "freeze rejects component registration",
        gecs_Component_register(context.world, "Other", sizeof(int32_t), NULL) == 0
    );
    gunit_assert(
        "freeze rejects context registration",
        gecs_context_register(context.world, "Other", sizeof(int32_t), NULL) == 0
    );
)

GUNIT_TEST(frozen_world_rejects_new_entities_and_values,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create clear fixtures", gecs__test_clear_create(&context));
    context.entities[2] = gecs_spawn(context.world);
    gunit_assert("empty entity created before freeze", context.entities[2].id != 0);
    context.preserved_contexts[0] =
        gecs_context_register(context.world, "EmptyFrozenContext", sizeof(int32_t), gecs__test_clear_destructor);
    gunit_assert("empty context registered before freeze", context.preserved_contexts[0] != 0);
    gecs_World_freeze(context.world);
    int32_t value = 42;
    gunit_assert("freeze rejects entity creation", gecs_spawn(context.world).id == 0);
    gunit_assert(
        "freeze rejects new component values",
        gecs_Component_set(context.entities[2], context.position, &value) == NULL
    );
    gunit_assert(
        "freeze rejects new context values",
        gecs_context_set(context.world, context.preserved_contexts[0], &value) == NULL
    );
    const int32_t* existing = gecs_Component_get(context.entities[0], context.position);
    gunit_assert("existing value remains readable", existing != NULL && *existing == 42);
)

GUNIT_TEST(frozen_world_can_be_cleared,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create clear fixtures", gecs__test_clear_create(&context));
    gecs_World_freeze(context.world);
    gecs_World_clear(context.world);
    gunit_assert(
        "clear still works when frozen",
        gecs__test_clear_destroyed == 3 && !gecs_Entity_alive(context.entities[0])
    );
    gecs__test_context_free(&context);
    gunit_assert("free does not destroy twice", gecs__test_clear_destroyed == 3);
)

GUNIT_TEST(entity_destroy_releases_owned_value_with_or_without_observer,
    gecs__TestContext context = {0};
    gecs_Entity entity = {0};

    GUNIT_SETUP({
        gunit_defer(gecs__test_context_free, &context);
        gunit_assert("create test world", gecs__test_context_create(&context));
        gunit_assert("register owned value fixtures", gecs__test_owned_create(&context));
        entity = gecs_spawn(context.world);
        gunit_assert("entity spawned", entity.id != 0);
        gunit_assert("store owned fixture value", gecs__test_put_owned(&context, entity, false, 17));
        gunit_assert(
            "observer registered",
            gecs_World_observe_destroy(context.world, gecs__test_observe_destroy, &context.observer)
        );
    })

    GUNIT_SECTION("registered observer receives the destroyed entity", {
        gecs_Entity_destroy(entity);
        gunit_assert("observer notified once", context.observer.calls == 1);
        gunit_assert(
            "observer receives destroyed entity handle",
            context.observer.last_entity.world == entity.world && context.observer.last_entity.id == entity.id
        );
        gunit_assert("entity no longer alive", !gecs_Entity_alive(entity));
        gunit_assert("owned component destroyed once", gecs__test_destroyed == 1);
    })

    GUNIT_SECTION("removed observer receives no notification", {
        gecs_World_unobserve_destroy(context.world, gecs__test_observe_destroy, &context.observer);
        gecs_Entity_destroy(entity);
        gunit_assert("removed observer is not notified", context.observer.calls == 0);
        gunit_assert("unobserved entity no longer alive", !gecs_Entity_alive(entity));
        gunit_assert("unobserved owned value destroyed once", gecs__test_destroyed == 1);
    })
)

GUNIT_TEST(world_allocation_failure_releases_memory,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gecs__test_context_allocator(&context);
    context.allocator.fail = true;
    context.world = gecs_World_new(context.alloc);
    gunit_assert("world allocation failure reported", context.world == NULL);
    gunit_assert("failed world creation releases memory", context.allocator.live == 0);
)

GUNIT_TEST(component_registration_with_failing_allocator_is_consistent,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    context.allocator.fail = true;
    gecs_Component registered = gecs_Component_register(context.world, "candidate", sizeof(gecs__TestBytes), NULL);
    if (registered == 0) {
        gunit_assert("failed registration leaves no type", gecs_Component_find(context.world, "candidate") == 0);
    } else {
        gunit_assert(
            "successful allocation-free registration is discoverable",
            gecs_Component_find(context.world, "candidate") == registered
        );
    }
    context.allocator.fail = false;
    gecs_Component retried = gecs_Component_register(context.world, "candidate", sizeof(gecs__TestBytes), NULL);
    gunit_assert("registration works when allocator recovers", retried != 0);
    if (registered != 0) {
        gunit_assert("retry resolves successfully registered type", retried == registered);
    }
)

GUNIT_TEST(component_self_assignment_preserves_contents,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create byte component fixtures", gecs__test_bytes_create(&context));
    const gecs__TestBytes* source = gecs_Component_get(context.entities[0], context.bytes);
    gunit_assert("original component is present", source != NULL);
    const gecs__TestBytes expected = *source;
    gunit_assert("self assignment succeeds", gecs_Component_set(context.entities[0], context.bytes, source) != NULL);
    const gecs__TestBytes* stored = gecs_Component_get(context.entities[0], context.bytes);
    gunit_assert(
        "self assignment preserves complete value",
        stored != NULL && memcmp(stored, &expected, sizeof(expected)) == 0
    );
)

GUNIT_TEST(component_replacement_with_failing_allocator_preserves_value,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create byte component fixtures", gecs__test_bytes_create(&context));
    const gecs__TestBytes first = {{0, 1, 2, 3, 4, 5, 6, 7}};
    const gecs__TestBytes second = {{8, 9, 10, 11, 12, 13, 14, 15}};
    context.allocator.fail = true;
    void* replaced = gecs_Component_set(context.entities[0], context.bytes, &second);
    const gecs__TestBytes* expected = replaced != NULL ? &second : &first;
    const gecs__TestBytes* stored = gecs_Component_get(context.entities[0], context.bytes);
    gunit_assert(
        "replacement retains old value on failure or new value on success",
        stored != NULL && memcmp(stored, expected, sizeof(*expected)) == 0
    );
)

GUNIT_TEST(component_copy_accepts_complete_value_from_another_entity,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create byte component fixtures", gecs__test_bytes_create(&context));
    const gecs__TestBytes* source = gecs_Component_get(context.entities[1], context.bytes);
    gunit_assert("source component is present", source != NULL);
    const gecs__TestBytes expected = *source;
    gunit_assert(
        "copy complete component value",
        gecs_Component_set(context.entities[0], context.bytes, source) != NULL
    );
    const gecs__TestBytes* destination = gecs_Component_get(context.entities[0], context.bytes);
    source = gecs_Component_get(context.entities[1], context.bytes);
    gunit_assert(
        "destination contains copied bytes",
        destination != NULL && memcmp(destination, &expected, sizeof(expected)) == 0
    );
    gunit_assert(
        "source entity value remains unchanged",
        source != NULL && memcmp(source, &expected, sizeof(expected)) == 0
    );
)

GUNIT_TEST(component_insertions_preserve_aliased_values_with_failing_allocator,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create byte component fixtures", gecs__test_bytes_create(&context));
    const size_t count = sizeof(context.entities) / sizeof(context.entities[0]);
    for (size_t i = 2; i < count; i++) {
        context.allocator.fail = false;
        context.entities[i] = gecs_spawn(context.world);
        gunit_assert("new target entity spawned", context.entities[i].id != 0);
        const gecs__TestBytes* source = gecs_Component_get(context.entities[0], context.bytes);
        gunit_assert("source component is present", source != NULL);
        const gecs__TestBytes expected = *source;
        size_t before = gecs_Component_count(context.world, context.bytes);
        context.allocator.fail = true;
        void* inserted = gecs_Component_set(context.entities[i], context.bytes, source);
        if (inserted == NULL) {
            gunit_assert(
                "failed insertion preserves component count",
                gecs_Component_count(context.world, context.bytes) == before
            );
            source = gecs_Component_get(context.entities[0], context.bytes);
            gunit_assert(
                "failed insertion preserves source value",
                source != NULL && memcmp(source, &expected, sizeof(expected)) == 0
            );
            context.allocator.fail = false;
            inserted = gecs_Component_set(context.entities[i], context.bytes, source);
            gunit_assert("failed insertion can retry when allocator recovers", inserted != NULL);
        }
        const gecs__TestBytes* stored = gecs_Component_get(context.entities[i], context.bytes);
        gunit_assert(
            "successful insertion adds one component",
            gecs_Component_count(context.world, context.bytes) == before + 1
        );
        gunit_assert(
            "successful insertion copies complete source value",
            stored != NULL && memcmp(stored, &expected, sizeof(expected)) == 0
        );
    }
)

GUNIT_TEST(context_self_assignment_preserves_contents,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    context.context = gecs_context_register(context.world, "Bytes", sizeof(gecs__TestBytes), gecs__test_clear_bytes);
    gunit_assert("context registered", context.context != 0);
    const gecs__TestBytes expected = {{0, 1, 2, 3, 4, 5, 6, 7}};
    void* source = gecs_context_set(context.world, context.context, &expected);
    gunit_assert("context created", source != NULL);
    gunit_assert("context self assignment succeeds", gecs_context_set(context.world, context.context, source) != NULL);
    const gecs__TestBytes* stored = gecs_context_get(context.world, context.context);
    gunit_assert(
        "context self assignment preserves complete value",
        stored != NULL && memcmp(stored, &expected, sizeof(expected)) == 0
    );
)

GUNIT_TEST(context_replacement_with_failing_allocator_preserves_value,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    context.context = gecs_context_register(context.world, "Bytes", sizeof(gecs__TestBytes), gecs__test_clear_bytes);
    gunit_assert("context registered", context.context != 0);
    const gecs__TestBytes first = {{0, 1, 2, 3, 4, 5, 6, 7}};
    const gecs__TestBytes second = {{8, 9, 10, 11, 12, 13, 14, 15}};
    gunit_assert("context created", gecs_context_set(context.world, context.context, &first) != NULL);
    context.allocator.fail = true;
    void* replaced = gecs_context_set(context.world, context.context, &second);
    const gecs__TestBytes* expected = replaced != NULL ? &second : &first;
    const gecs__TestBytes* stored = gecs_context_get(context.world, context.context);
    gunit_assert(
        "context retains old value on failure or new value on success",
        stored != NULL && memcmp(stored, expected, sizeof(*expected)) == 0
    );
    context.allocator.fail = false;
    gunit_assert("tracked context can be removed", gecs_context_remove(context.world, context.context));
)

GUNIT_TEST(removing_dense_components_empties_pool,
    gecs__TestContext context = {0};
    gunit_defer(gecs__test_context_free, &context);
    gunit_assert("create test world", gecs__test_context_create(&context));
    gunit_assert("create byte component fixtures", gecs__test_bytes_create(&context));
    while (gecs_Component_count(context.world, context.bytes) > 0) {
        gecs_Entity entity = gecs_Component_entity(context.world, context.bytes, 0);
        gunit_assert("remove last dense component", entity.id != 0 && gecs_Component_remove(entity, context.bytes));
    }
    gunit_assert("empty pool has zero count", gecs_Component_count(context.world, context.bytes) == 0);
    gecs__test_context_free(&context);
    gunit_assert("tracked world releases all allocations", context.allocator.live == 0);
)

GUNIT_TEST(selective_world_clear_preserves_contexts,
    gecs__TestContext context = {0};

    GUNIT_SETUP({
        gunit_defer(gecs__test_context_free, &context);
        gunit_assert("create test world", gecs__test_context_create(&context));
        gunit_assert("create staged clear fixtures", gecs__test_staged_clear_create(&context));
    })

    GUNIT_SECTION("entity-only clear preserves every context", {
        gecs_World_clear_entities(context.world);
        gunit_assert("all entity component values destroyed", gecs__test_clear_destroyed == 2);
        gunit_assert(
            "entity handles invalidated",
            !gecs_Entity_alive(context.entities[0]) && !gecs_Entity_alive(context.entities[1])
        );
        gunit_assert("entity components removed", gecs_Component_count(context.world, context.position) == 0);
        const int32_t* ordinary = gecs_context_get(context.world, context.context);
        const int32_t* first = gecs_context_get(context.world, context.preserved_contexts[0]);
        const int32_t* second = gecs_context_get(context.world, context.preserved_contexts[1]);
        gunit_assert("ordinary context preserved", ordinary != NULL && *ordinary == 42);
        gunit_assert("first selected context preserved", first != NULL && *first == 11);
        gunit_assert("second selected context preserved", second != NULL && *second == 22);
    })

    GUNIT_SECTION("clear preserves one selected context", {
        gecs_World_clear_except(context.world, context.preserved_contexts[0]);
        gunit_assert("entities and unselected contexts destroyed", gecs__test_clear_destroyed == 4);
        gunit_assert(
            "entity handles invalidated",
            !gecs_Entity_alive(context.entities[0]) && !gecs_Entity_alive(context.entities[1])
        );
        const int32_t* kept = gecs_context_get(context.world, context.preserved_contexts[0]);
        gunit_assert("selected context preserved", kept != NULL && *kept == 11);
        gunit_assert("ordinary context removed", gecs_context_get(context.world, context.context) == NULL);
        gunit_assert("other context removed", gecs_context_get(context.world, context.preserved_contexts[1]) == NULL);
        gecs_World_clear(context.world);
        gunit_assert("later clear destroys retained context once", gecs__test_clear_destroyed == 5);
    })

    GUNIT_SECTION("clear preserves multiple selected contexts", {
        gecs_World_clear_except_many(context.world, context.preserved_contexts, 2);
        gunit_assert("entities and unselected context destroyed", gecs__test_clear_destroyed == 3);
        gunit_assert(
            "entity handles invalidated",
            !gecs_Entity_alive(context.entities[0]) && !gecs_Entity_alive(context.entities[1])
        );
        const int32_t* first = gecs_context_get(context.world, context.preserved_contexts[0]);
        const int32_t* second = gecs_context_get(context.world, context.preserved_contexts[1]);
        gunit_assert("first selected context preserved", first != NULL && *first == 11);
        gunit_assert("second selected context preserved", second != NULL && *second == 22);
        gunit_assert("unselected context removed", gecs_context_get(context.world, context.context) == NULL);
        gecs_World_clear(context.world);
        gunit_assert("later clear destroys retained contexts once", gecs__test_clear_destroyed == 5);
    })
)

gunit_main();

static void gecs__test_destroy_owned(void* p) {
    gecs__TestOwned* owned = p;
    galloc_free(owned->alloc, owned->ptr);
    gecs__test_destroyed += 1;
}

static void* gecs__test_alloc(void* data, size_t size) {
    gecs__TestAllocator* state = data;
    if (state->fail) {
        return NULL;
    }

    void* p = galloc_alloc(state->backing, size);
    if (p != NULL) {
        state->live += 1;
    }

    return p;
}

static void gecs__test_free(void* data, void* ptr) {
    gecs__TestAllocator* state = data;
    if (!gunit_check("free has a live allocation", state->live > 0)) {
        return;
    }
    state->live -= 1;
    galloc_free(state->backing, ptr);
}

static void* gecs__test_calloc(void* data, size_t count, size_t size) {
    gecs__TestAllocator* state = data;
    if (state->fail) {
        return NULL;
    }

    void* p = galloc_calloc(state->backing, count, size);
    if (p != NULL) {
        state->live += 1;
    }

    return p;
}

static void* gecs__test_realloc(void* data, void* ptr, size_t old_size, size_t new_size) {
    gecs__TestAllocator* state = data;
    if (state->fail) {
        return NULL;
    }

    bool fresh = ptr == NULL;
    void* result = galloc_realloc(state->backing, ptr, old_size, new_size);
    if (result != NULL && fresh) {
        state->live += 1;
    }

    return result;
}

static void gecs__test_clear_bytes(void* value) {
    memset(value, 0, sizeof(gecs__TestBytes));
}

static void gecs__test_clear_destructor(void* value) {
    (void)value;
    gecs__test_clear_destroyed += 1;
}

static void gecs__test_observe_destroy(gecs_Entity handle, void* user) {
    gecs__TestObserver* observer = user;
    observer->last_entity = handle;
    observer->calls += 1;
}

static void gecs__test_context_allocator(gecs__TestContext* context) {
    static const galloc_FunctionTable table =
        {gecs__test_alloc, gecs__test_free, gecs__test_calloc, gecs__test_realloc};
    context->allocator.backing = galloc_malloc_allocator();
    context->alloc = (galloc_Allocator) {.data = &context->allocator, .ftable = &table};
    gecs__test_destroyed = 0;
    gecs__test_clear_destroyed = 0;
}

static bool gecs__test_context_create(gecs__TestContext* context) {
    gecs__test_context_allocator(context);
    context->world = gecs_World_new(context->alloc);
    return gunit_check("world created", context->world != NULL);
}

static void gecs__test_context_free(void* user) {
    gecs__TestContext* context = user;
    context->allocator.fail = false;
    galloc_free(context->alloc, context->pending_owned);
    context->pending_owned = NULL;
    if (context->world != NULL) {
        gecs_World_unobserve_destroy(context->world, gecs__test_observe_destroy, &context->observer);
        gecs_World_free(context->world);
        context->world = NULL;
    }
    if (context->second_world != NULL) {
        gecs_World_free(context->second_world);
        context->second_world = NULL;
    }
}

static bool gecs__test_positions_create(gecs__TestContext* context, size_t count) {
    context->position = GECS_COMPONENT(context->world, gecs__TestPosition);
    if (!gunit_check("position registered", context->position != 0)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        gecs__TestPosition position = {.x = (int32_t)i + 3, .y = 4};
        context->entities[i] = gecs_spawn(context->world);
        if (!gunit_check("entity spawned", context->entities[i].id != 0)) {
            return false;
        }
        if (!gunit_check(
                "position created",
                gecs_Component_set(context->entities[i], context->position, &position) != NULL
            )) {
            return false;
        }
    }
    return true;
}

static bool gecs__test_owned_create(gecs__TestContext* context) {
    context->owned =
        gecs_Component_register(context->world, "Owned", sizeof(gecs__TestOwned), gecs__test_destroy_owned);
    context->context =
        gecs_context_register(context->world, "Owned", sizeof(gecs__TestOwned), gecs__test_destroy_owned);
    return gunit_check("owned types registered", context->owned != 0 && context->context != 0);
}

static bool gecs__test_put_owned(gecs__TestContext* context, gecs_Entity entity, bool in_context, int32_t value) {
    context->pending_owned = galloc_alloc(context->alloc, sizeof(int32_t));
    if (!gunit_check("owned test value allocated", context->pending_owned != NULL)) {
        return false;
    }
    *context->pending_owned = value;
    gecs__TestOwned owned = {.alloc = context->alloc, .ptr = context->pending_owned};
    void* stored = in_context
        ? gecs_context_set(context->world, context->context, &owned)
        : gecs_Component_set(entity, context->owned, &owned);
    if (stored != NULL) {
        context->pending_owned = NULL;
    }
    return gunit_check("owned value stored", stored != NULL);
}

static bool gecs__test_clear_create(gecs__TestContext* context) {
    context->position =
        gecs_Component_register(context->world, "ClearComponent", sizeof(int32_t), gecs__test_clear_destructor);
    context->context =
        gecs_context_register(context->world, "ClearContext", sizeof(int32_t), gecs__test_clear_destructor);
    if (!gunit_check("clear types registered", context->position != 0 && context->context != 0)) {
        return false;
    }
    int32_t value = 42;
    for (size_t i = 0; i < 2; i++) {
        context->entities[i] = gecs_spawn(context->world);
        if (!gunit_check("clear entity spawned", context->entities[i].id != 0)) {
            return false;
        }
        if (!gunit_check(
                "clear component created",
                gecs_Component_set(context->entities[i], context->position, &value) != NULL
            )) {
            return false;
        }
    }
    return gunit_check("clear context created", gecs_context_set(context->world, context->context, &value) != NULL);
}

static bool gecs__test_bytes_create(gecs__TestContext* context) {
    context->bytes = gecs_Component_register(context->world, "Bytes", sizeof(gecs__TestBytes), gecs__test_clear_bytes);
    if (!gunit_check("bytes type registered", context->bytes != 0)) {
        return false;
    }
    for (size_t i = 0; i < 2; i++) {
        context->entities[i] = gecs_spawn(context->world);
        if (!gunit_check("byte entity spawned", context->entities[i].id != 0)) {
            return false;
        }
    }
    gecs__TestBytes first = {{0, 1, 2, 3, 4, 5, 6, 7}};
    gecs__TestBytes second = {{8, 9, 10, 11, 12, 13, 14, 15}};
    if (!gunit_check(
            "first byte component created",
            gecs_Component_set(context->entities[0], context->bytes, &first) != NULL
        )) {
        return false;
    }
    return gunit_check(
        "second byte component created",
        gecs_Component_set(context->entities[1], context->bytes, &second) != NULL
    );
}

static bool gecs__test_staged_clear_create(gecs__TestContext* context) {
    if (!gecs__test_clear_create(context)) {
        return false;
    }
    const char* names[] = {"FirstRetainedContext", "SecondRetainedContext"};
    const int32_t values[] = {11, 22};
    for (size_t i = 0; i < 2; i++) {
        context->preserved_contexts[i] =
            gecs_context_register(context->world, names[i], sizeof(int32_t), gecs__test_clear_destructor);
        if (!gunit_check("retained context type registered", context->preserved_contexts[i] != 0)) {
            return false;
        }
        if (!gunit_check(
                "retained context value created",
                gecs_context_set(context->world, context->preserved_contexts[i], &values[i]) != NULL
            )) {
            return false;
        }
    }
    return true;
}

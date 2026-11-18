#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// IDs are local to a world; zero is always invalid. Entities include a
// generation so that a deleted entity cannot accidentally address a new one.
typedef uint64_t gecs_ID;
typedef uint32_t gecs_Component;
typedef struct gecs_World gecs_World;

typedef struct gecs_Entity {
    gecs_World* world;
    gecs_ID id;
} gecs_Entity;

typedef void (*gecs_DestroyFn)(void* value);
typedef void (*gecs_EntityPredestroyFn)(gecs_Entity entity, void* user);

gecs_World* gecs_World_new(galloc_Allocator alloc);

galloc_Allocator glint_allocator(const gecs_World* world);

/// Borrow the world's temporary allocator. Allocations expire at frame end.
galloc_Allocator glint_temp_allocator(gecs_World* world);

/// Invalidate the world's temporary allocations for reuse
void gecs_World_reset_temp(gecs_World* world);

/// Clear the world while preserving specified contexts for staged shutdown.
void gecs_World_clear_except_many(gecs_World* world, const gecs_Component* contexts, size_t count);

/// Clear the world while preserving one context for staged shutdown.
void gecs_World_clear_except(gecs_World* world, gecs_Component context);

/// Destroy entities while leaving world contexts available.
void gecs_World_clear_entities(gecs_World* world);

/// Destroy all entities and context values
void gecs_World_clear(gecs_World* world);

/// Prevent new entities, registrations, and values
void gecs_World_freeze(gecs_World* world);

void gecs_World_free(gecs_World* world);

/// Spawn an entity; a zero ID indicates failure.
gecs_Entity gecs_spawn(gecs_World* world);

bool gecs_Entity_alive(gecs_Entity entity);

bool gecs_World_observe_destroy(gecs_World* world, gecs_EntityPredestroyFn callback, void* user);

void gecs_World_unobserve_destroy(gecs_World* world, gecs_EntityPredestroyFn callback, void* user);

void gecs_Entity_destroy(gecs_Entity entity);

/// Register once per name when size and destructor match.
///
/// # Errors
///
/// Returns zero on allocation failure or incompatible registration.
gecs_Component gecs_Component_register(gecs_World* world, const char* name, size_t size, gecs_DestroyFn destroy);

gecs_Component gecs_Component_find(const gecs_World* world, const char* name);

/// Copy the component value, or zero-initialize it when `value` is NULL.
void* gecs_Component_set(gecs_Entity entity, gecs_Component component, const void* value);

void* gecs_Component_get(gecs_Entity entity, gecs_Component component);

bool gecs_Component_remove(gecs_Entity entity, gecs_Component component);

size_t gecs_Component_count(const gecs_World* world, gecs_Component component);

/// Get the entity at a component's dense index
gecs_Entity gecs_Component_entity(gecs_World* world, gecs_Component component, size_t dense_index);

/// Register a world context type
///
/// Context values are singleton values registered separately from components.
gecs_Component gecs_context_register(gecs_World* world, const char* name, size_t size, gecs_DestroyFn destroy);

/// Look up a registered context, including while the world is frozen.
gecs_Component gecs_context_find(const gecs_World* world, const char* name);

void* gecs_context_set(gecs_World* world, gecs_Component context, const void* value);

void* gecs_context_get(const gecs_World* world, gecs_Component context);

bool gecs_context_remove(gecs_World* world, gecs_Component context);

#define GECS_COMPONENT(world, type) gecs_Component_register((world), #type, sizeof(type), NULL)
#define GECS_CONTEXT(world, type) gecs_context_register((world), #type, sizeof(type), NULL)

#ifdef __cplusplus
}
#endif

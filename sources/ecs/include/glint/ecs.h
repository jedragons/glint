#ifndef GECS_ECS_H
#define GECS_ECS_H

#include <glint/alloc.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IDs are local to a world; zero is always invalid. Entities include a
 * generation so that a deleted entity cannot accidentally address a new one. */
typedef uint64_t gecs_Entity;
typedef uint32_t gecs_Component;
typedef struct gecs_World gecs_World;
typedef void (*gecs_DestroyFn)(void *value);
typedef void (*gecs_EntityPredestroyFn)(gecs_World *world, gecs_Entity entity, void *user);

/* Pass the same allocator to all operations on a world that allocate or free.
 * Its state and function table must outlive the world.
 * Component and context values are byte-relocatable. Pointers to stored values
 * can be invalidated by inserting/removing that type, or by destroying an
 * entity. A destructor owns any resources held by the value; it must not
 * modify the world. Values are shallow-copied, so their ownership must be
 * transferred by the caller and their resources must not also be owned by
 * another stored value. */
gecs_World *gecs_World_new(galloc_Allocator alloc);
/* Destroy all entities and context values, retaining registrations and the
 * world object. Existing entity handles become invalid. May be called again. */
void gecs_World_clear(galloc_Allocator alloc, gecs_World *world);
/* Permanently prevent entity creation, registration and setting values. Reads,
 * removals, clear and destruction remain available for shutdown. */
void gecs_World_freeze(gecs_World *world);
void gecs_World_free(galloc_Allocator alloc, gecs_World *world);
gecs_Entity gecs_Entity_new(galloc_Allocator alloc, gecs_World *world);
bool gecs_Entity_alive(const gecs_World *world, gecs_Entity entity);
/* One optional observer per world. Registration fails if an observer is already
 * installed; unregister only with the matching callback and user. It runs before
 * component removal, while the entity is alive. Recursive destruction of the
 * same entity is ignored. Clear/free during a destruction callback is ignored;
 * creating entities during clear is forbidden. The observer must be unregistered
 * before its user data is freed. */
bool gecs_World_observe_destroy(gecs_World *world, gecs_EntityPredestroyFn callback, void *user);
void gecs_World_unobserve_destroy(gecs_World *world, gecs_EntityPredestroyFn callback, void *user);
void gecs_Entity_destroy(gecs_World *world, gecs_Entity entity);

/* Registration is idempotent by name when size and destructor match; returns
 * zero on allocation failure or incompatible re-registration. */
gecs_Component gecs_Component_register(
    galloc_Allocator alloc,
    gecs_World *world,
    const char *name,
    size_t size,
    gecs_DestroyFn destroy
);
gecs_Component gecs_Component_find(const gecs_World *world, const char *name);
/* Set copies size bytes from value, or zero-initializes when value is NULL.
 * Replacing an existing value destroys it first, except that setting from the
 * same stored address is a no-op. Inputs to a replacement must not depend on
 * resources released by the old value's destructor. On allocation failure,
 * the old value is unchanged and NULL is returned. */
void *gecs_Component_set(
    galloc_Allocator alloc,
    gecs_World *world,
    gecs_Entity entity,
    gecs_Component component,
    const void *value
);
void *gecs_Component_get(const gecs_World *world, gecs_Entity entity, gecs_Component component);
bool gecs_Component_remove(gecs_World *world, gecs_Entity entity, gecs_Component component);
size_t gecs_Component_count(const gecs_World *world, gecs_Component component);
/* Removal moves the last dense entry into the removed slot. When removing
 * during iteration, process that index again before advancing. */
gecs_Entity gecs_Component_entity(const gecs_World *world, gecs_Component component, size_t dense_index);

/* Context values are singleton values registered separately from components. */
gecs_Component gecs_context_register(
    galloc_Allocator alloc,
    gecs_World *world,
    const char *name,
    size_t size,
    gecs_DestroyFn destroy
);
void *gecs_context_set(galloc_Allocator alloc, gecs_World *world, gecs_Component context, const void *value);
void *gecs_context_get(const gecs_World *world, gecs_Component context);
bool gecs_context_remove(galloc_Allocator alloc, gecs_World *world, gecs_Component context);

#define GECS_COMPONENT(alloc, world, type) gecs_Component_register((alloc), (world), #type, sizeof(type), NULL)
#define GECS_CONTEXT(alloc, world, type) gecs_context_register((alloc), (world), #type, sizeof(type), NULL)

#ifdef __cplusplus
}
#endif
#endif

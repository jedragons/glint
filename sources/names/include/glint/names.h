#ifndef GNAME_NAMES_H
#define GNAME_NAMES_H

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <stdbool.h>

/* Names are unique within a world. Setting a null name removes it. */
bool gname_set(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, const char *name);
const char *gname_get(const gecs_World *world, gecs_Entity entity);
gecs_Entity gname_find(const gecs_World *world, const char *name);
/* True when a live entity currently carries the name. */
bool gname_exists(const gecs_World *world, const char *name);
/* Renames an entity that already has a name; false when the entity is dead,
 * unnamed, or the new name is taken. Renaming to the entity's own current
 * name succeeds unchanged. */
bool gname_rename(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, const char *new_name);
/* Removes an entity's name; false when there was nothing to remove. Removing
 * by name is find followed by remove. */
bool gname_remove(gecs_World *world, gecs_Entity entity);

#endif // GNAME_NAMES_H

#pragma once

#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Set or remove an entity's name
///
/// Names are unique within a world. Setting a null name removes it.
bool gname_set(gecs_Entity entity, const char* name);

const char* gname_get(gecs_Entity entity);

gecs_Entity gname_find(gecs_World* world, const char* name);

bool gname_exists(gecs_World* world, const char* name);

/// Rename an already named entity.
///
/// # Errors
///
/// Returns false for dead or unnamed entities, or when the name is taken.
bool gname_rename(gecs_Entity entity, const char* new_name);

/// Returns false if the entity has no name.
bool gname_remove(gecs_Entity entity);

#ifdef __cplusplus
}
#endif

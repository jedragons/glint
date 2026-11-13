#ifndef GBEH_BEHAVIORS_H
#define GBEH_BEHAVIORS_H

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <glint/script.h>
#include <glint/vfs.h>
#include <stdbool.h>

typedef struct gbeh_Behaviors gbeh_Behaviors;

/* The world, script and VFS must outlive this manager. Each entity can have one
 * behavior; setup runs on attach, update each frame, and destroy on removal.
 * Errors from failed operations remain on the script's Lua stack. Native ECS
 * destruction cannot report hook errors; it discards them and still destroys
 * the entity. Only one destroy observer/behavior manager may own a world.
 * Pass the same allocator to all manager operations; it must outlive it. */
gbeh_Behaviors *gbeh_new(galloc_Allocator alloc, gscript_Script *script, gecs_World *world, gvfs_Vfs *vfs);
bool gbeh_attach(galloc_Allocator alloc, gbeh_Behaviors *behaviors, gecs_Entity entity, const char *path);
bool gbeh_update(galloc_Allocator alloc, gbeh_Behaviors *behaviors);
/* Calls remaining destroy hooks, then releases the manager even if a hook fails. */
void gbeh_destroy(galloc_Allocator alloc, gbeh_Behaviors *behaviors);

#endif // GBEH_BEHAVIORS_H

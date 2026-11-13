#ifndef GSCRIPT_SCRIPT_H
#define GSCRIPT_SCRIPT_H

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct lua_State lua_State;
typedef struct gscript_Script gscript_Script;
/* Return false and leave a Lua error on the stack to abort a Lua entity:destroy(). */
typedef bool (*gscript_AttachFn)(
    galloc_Allocator alloc,
    gecs_World *world,
    gecs_Entity entity,
    const char *path,
    void *user
);
typedef bool (*gscript_DestroyFn)(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, void *user);

/* The allocator and its data must outlive the Lua state. Use the same allocator
 * for all operations on a state. Errors remain on the Lua stack. */
gscript_Script *gscript_new(galloc_Allocator alloc);
void gscript_destroy(galloc_Allocator alloc, gscript_Script *script);
bool gscript_run(galloc_Allocator alloc, gscript_Script *script, const char *source, size_t length, const char *name);
lua_State *gscript_state(gscript_Script *script);
/* Bind spawn() and logging to a world. The world must outlive the state. */
bool gscript_bind_world(galloc_Allocator alloc, gscript_Script *script, gecs_World *world);
/* Called before Lua destroys an entity. Recursive destruction of the same
 * entity is suppressed. Passing NULL removes the callback. */
void gscript_set_entity_destroy(gscript_Script *script, gscript_DestroyFn callback, void *user);
void gscript_set_entity_attach(gscript_Script *script, gscript_AttachFn callback, void *user);
/* Release Lua fields after destruction performed outside the Lua API. */
void gscript_forget_entity(galloc_Allocator alloc, gscript_Script *script, gecs_World *world, gecs_Entity entity);
/* Load a Lua chunk returning a behavior table. Returns LUA_NOREF on error;
 * errors remain on the Lua stack. Callbacks receive an Entity userdata. */
int gscript_load_module(galloc_Allocator alloc, gscript_Script *script, const char *source, size_t length, const char *name);
bool gscript_call_module(
    galloc_Allocator alloc,
    gscript_Script *script,
    int module,
    const char *callback,
    gecs_World *world,
    gecs_Entity entity
);
void gscript_release_module(galloc_Allocator alloc, gscript_Script *script, int module);

#endif // GSCRIPT_SCRIPT_H

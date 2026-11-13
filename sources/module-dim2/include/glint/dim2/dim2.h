#ifndef GDIM2_DIM2_H_
#define GDIM2_DIM2_H_

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <glint/vfs.h>
#include <glint/modules.h>
#include <glint/script.h>
#include <glint/types.h>

#include <stdbool.h>

typedef struct lua_State lua_State;
typedef struct gdim2_Dim2 gdim2_Dim2;

/* Supply before loading gdim2_module. All pointers must outlive the
 * loaded module. A disabled host installs an empty service (for headless use).
 * A host setting defer_destroy assumes ownership of the resulting instance
 * after unload, and must destroy it after clearing the world and Lua state. */
typedef struct gdim2_Host {
    gvfs_Vfs *vfs;
    gscript_Script *script;
    const float *delta_time;
    bool enabled;
    bool defer_destroy;
} gdim2_Host;
typedef struct gdim2_Service {
    gdim2_Dim2 *instance;
    const float *delta_time;
    bool defer_destroy;
} gdim2_Service;
#define GDIM2_MODULE_NAME "glint.dim2"
/* Requires gdim2_Host and gtypes_LuaService. Imports the types module as a
 * dependency. Load creates and binds 2D;
 * update and render dispatch animation and drawing. Render requires the host
 * to have begun drawing. Unload destroys it unless ownership was deferred. */
gmod_Module gdim2_module(void);

typedef struct gdim2_Transform2D {
    gvec2_Vec2 position;
    gvec2_Vec2 scale;
    gvec2_Vec2 origin;
    float rotation;
} gdim2_Transform2D;

/* The world, VFS and allocator must outlive this module. A raylib graphics
 * context must exist before loading sprite textures. Use one instance per
 * world. Destroy this module before closing the graphics context. Existing
 * Lua bindings and references raise an error after module destruction. The
 * allocator must outlive those references. Bind Lua inside a protected call
 * if allocation errors need to be caught. */
gdim2_Dim2 *gdim2_new(galloc_Allocator alloc, gecs_World *world, gvfs_Vfs *vfs);
void gdim2_destroy(galloc_Allocator alloc, gdim2_Dim2 *dim2);
bool gdim2_bind_lua(galloc_Allocator alloc, gdim2_Dim2 *dim2, lua_State *lua);
/* Polls asset changes and reloads valid replacements before animation update. */
void gdim2_update(galloc_Allocator alloc, gdim2_Dim2 *dim2, float delta);
void gdim2_render(gdim2_Dim2 *dim2);

#endif // GDIM2_DIM2_H_

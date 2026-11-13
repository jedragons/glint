#ifndef GSCRIPT_INTERNAL_H
#define GSCRIPT_INTERNAL_H

#include <glint/script.h>
#include <lua.h>

void gscript__entity_bind_names(galloc_Allocator alloc, lua_State *lua, gecs_World *world);
void gscript__entity_set_script(lua_State *lua, gscript_Script *script);
void gscript__entity_forget(lua_State *lua, gecs_World *world, gecs_Entity entity);
int gscript__dispatch_entity_attach(
    lua_State *lua,
    gscript_Script *script,
    gecs_World *world,
    gecs_Entity entity,
    const char *path
);
gscript_Script *gscript__entity_script(lua_State *lua);
int gscript__dispatch_entity_destroy(lua_State *lua, gscript_Script *script, gecs_World *world, gecs_Entity entity);

#endif // GSCRIPT_INTERNAL_H

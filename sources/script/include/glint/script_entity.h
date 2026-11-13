#ifndef GSCRIPT_SCRIPT_ENTITY_H
#define GSCRIPT_SCRIPT_ENTITY_H

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <lua.h>

void gscript_register_entity(galloc_Allocator alloc, lua_State *lua);

/* Entity values are handles into world, which must outlive the Lua state. */
void gscript_push_entity(galloc_Allocator alloc, lua_State *lua, gecs_World *world, gecs_Entity entity);
void gscript_check_entity(lua_State *lua, int index, gecs_World **world, gecs_Entity *entity);

#endif // GSCRIPT_SCRIPT_ENTITY_H

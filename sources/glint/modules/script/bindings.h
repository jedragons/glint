#pragma once

#include "./script.h"

#ifdef __cplusplus
extern "C" {
#endif

void gscript_bindings_register_entity(galloc_Allocator alloc, lua_State* lua);

/// Bind `log_*` globals. Call under Lua protection to catch allocation errors.
void gscript_bindings_register_logging(lua_State* lua);

/// Push an entity as Lua userdata
///
/// Entity values are handles into world, which must outlive the Lua state.
void gscript_bindings_push_entity(lua_State* lua, gecs_Entity entity);

gecs_Entity gscript_bindings_check_entity(lua_State* lua, int index);

/// Bind entity lookup and spawn() to a world. The world must outlive the state.
bool gscript_bindings_bind_world(gscript_lua_Runtime* script, gecs_World* world);

/// Release Lua fields after destruction performed outside the Lua API.
void gscript_bindings_forget_entity(gscript_lua_Runtime* script, gecs_Entity entity);

#ifdef __cplusplus
}
#endif

#ifndef GTYPES_TYPES_LUA_H
#define GTYPES_TYPES_LUA_H

#include <glint/alloc.h>
#include <glint/modules.h>
#include <stdbool.h>

typedef struct lua_State lua_State;

/* Host-provided Lua state, which must outlive all bindings installed in it. */
typedef struct gtypes_LuaService {
    lua_State *lua;
} gtypes_LuaService;
gmod_Module gtypes_module(void);

/* Bind Vec2, Rect and Color; Lua performs its own allocations through the
 * allocator configured on the state. Call under Lua protection if allocation
 * failure must be caught. The allocator must outlive the Lua state. */
bool gtypes_bind_lua(galloc_Allocator alloc, lua_State *lua);
/* Push a Vec2 with arithmetic and scale methods onto a bound state's stack. */
void gtypes_push_vec2(galloc_Allocator alloc, lua_State *lua, float x, float y);
/* Install the shared Vec2 operators and __vec2_scale method on a metatable.
 * Operands must expose numeric x and y fields. Does not change the stack. */
void gtypes_install_vec2_ops(galloc_Allocator alloc, lua_State *lua, int metatable);

#endif // GTYPES_TYPES_LUA_H

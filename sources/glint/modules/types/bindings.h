#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>

typedef struct lua_State lua_State;

#ifdef __cplusplus
extern "C" {
#endif

// Host-provided Lua state, which must outlive all bindings installed in it.
typedef struct gtypes_LuaService {
    lua_State* lua;
} gtypes_LuaService;

/// Bind Vec2, Rect and Color.
///
/// # Errors
///
/// Call under Lua protection to catch allocation errors.
bool gtypes_bindings_bind(galloc_Allocator alloc, lua_State* lua);

/// Push a Vec2 with arithmetic, scale, length and normalize methods.
void gtypes_bindings_push_vec2(galloc_Allocator alloc, lua_State* lua, float x, float y);

/// Install Vec2 operators and a __vec2_methods table on a metatable.
/// Operands must expose numeric x and y fields. Does not change the stack.
void gtypes_bindings_install_vec2_ops(galloc_Allocator alloc, lua_State* lua, int metatable);

/// Install bindings under Lua protection. On failure, leave the error on the stack.
bool gtypes_bindings_install(galloc_Allocator alloc, lua_State* lua);

#ifdef __cplusplus
}
#endif

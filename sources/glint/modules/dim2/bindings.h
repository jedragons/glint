#pragma once

#include "./dim2.h"

typedef struct lua_State lua_State;

#ifdef __cplusplus
extern "C" {
#endif

/// Bind inside a protected Lua call to catch allocation errors.
/// Sprite.set and AnimatedSprite.set reject invalid Lua arguments with an error
/// before replacing existing components.
bool gdim2_bindings_bind(galloc_Allocator alloc, gdim2_Dim2* dim2, lua_State* lua);

/// Install bindings under Lua protection. On failure, leave the error on the stack.
bool gdim2_bindings_install(galloc_Allocator alloc, gdim2_Dim2* dim2, lua_State* lua);

#ifdef __cplusplus
}
#endif

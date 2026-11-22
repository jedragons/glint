#pragma once

#include <glint/core/alloc.h>
#include <glint/ecs/ecs.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lua_State lua_State;
typedef struct gscript_lua_Runtime gscript_lua_Runtime;

// Return false and leave a Lua error on the stack to reject an attachment.
typedef bool (*gscript_lua_AttachFn)(gecs_Entity entity, const char* path, void* user);

// Return false and leave a Lua error on the stack to abort entity:destroy().
typedef bool (*gscript_lua_DestroyFn)(gecs_Entity entity, void* user);

/// Create a Lua runtime using `alloc` for its lifetime.
gscript_lua_Runtime* gscript_lua_new(galloc_Allocator alloc);

void gscript_lua_destroy(galloc_Allocator alloc, gscript_lua_Runtime* script);

/// Run a Lua chunk.
///
/// # Errors
///
/// Lua errors return false and remain on the stack.
bool gscript_lua_run(
    galloc_Allocator alloc,
    gscript_lua_Runtime* script,
    const char* source,
    size_t length,
    const char* name
);

lua_State* gscript_lua_state(gscript_lua_Runtime* script);

/// Copy and pop the Lua error left by a failed operation.
void gscript_lua_take_error(gscript_lua_Runtime* script, char* error, size_t error_size);

/// Run before Lua destroys an entity. Recursive calls on the same entity are suppressed.
/// NULL removes the callback.
void gscript_lua_set_entity_destroy(gscript_lua_Runtime* script, gscript_lua_DestroyFn callback, void* user);

void gscript_lua_set_entity_attach(gscript_lua_Runtime* script, gscript_lua_AttachFn callback, void* user);

/// Load a chunk returning a behavior table; callbacks receive Entity userdata.
///
/// # Errors
///
/// Returns LUA_NOREF and leaves the Lua error on the stack.
int gscript_lua_load_module(
    galloc_Allocator alloc,
    gscript_lua_Runtime* script,
    const char* source,
    size_t length,
    const char* name
);

/// Call a behavior callback.
///
/// # Errors
///
/// Lua errors return false and remain on the stack.
bool gscript_lua_call_module(gscript_lua_Runtime* script, int module, const char* callback, gecs_Entity entity);

void gscript_lua_release_module(galloc_Allocator alloc, gscript_lua_Runtime* script, int module);

#ifdef __cplusplus
}
#endif

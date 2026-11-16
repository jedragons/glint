#pragma once

#include <glint/modules/script/behaviors.h>
#include <glint/modules/script/script.h>

#include <lua.h>

typedef struct gscript_bindings__DestroyFrame gscript_bindings__DestroyFrame;

struct gscript_lua_Runtime {
    galloc_Allocator alloc;
    lua_State* lua;
    gscript_lua_AttachFn attach_callback;
    void* attach_user;
    gscript_lua_DestroyFn destroy_callback;
    void* destroy_user;
    gscript_bindings__DestroyFrame* destroying;
};

#ifdef __cplusplus
extern "C" {
#endif

bool gscript_behavior__update(galloc_Allocator alloc, gscript_behavior_Behaviors* behaviors, bool poll);

#ifdef __cplusplus
}
#endif

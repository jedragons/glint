#pragma once

#include <glint/modules/script/behaviors.h>
#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GSCRIPT_MODULE_NAME "glint.script"

typedef struct gscript_Service {
    gscript_lua_Runtime* script;
    gscript_behavior_Behaviors* behaviors;
    bool module_owned;
    bool host_updates;
} gscript_Service;

gmod_Module gscript_module(void);

#ifdef __cplusplus
}
#endif

#pragma once

#include <glint/modules/assets/cache.h>
#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GASSET_MODULE_NAME "glint.assets"

typedef struct gasset_Service {
    gasset_Cache* cache;
    bool module_owned;
    bool host_polls;
} gasset_Service;

gmod_Module gasset_module(void);

#ifdef __cplusplus
}
#endif

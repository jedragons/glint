#pragma once

#include "./texture_asset.h"

#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GTEXTURE_MODULE_NAME "glint.texture"

typedef struct gtexture_Service {
    gtexture_Assets* assets;
    bool module_owned;
} gtexture_Service;

gmod_Module gtexture_module(void);

#ifdef __cplusplus
}
#endif

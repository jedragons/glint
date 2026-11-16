#pragma once

#include "./render.h"

#include <glint/runtime/modules.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Requires an active graphics context when called.
typedef struct grender_Service {
    void (*draw_texture)(grender_Texture, grender_TextureDrawParams);
    void (*clear_background)(gcolor_Color);
    bool module_owned;
} grender_Service;

gmod_Module grender_module(void);

#ifdef __cplusplus
}
#endif

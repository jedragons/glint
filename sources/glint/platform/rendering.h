#pragma once

#include <glint/math/color.h>
#include <glint/math/rect.h>
#include <glint/math/vec2.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gplatform_Texture {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} gplatform_Texture;

typedef struct gplatform_TextureDrawParams {
    grect_Rect source;
    grect_Rect dest;
    gvec2_Vec2 origin;
    float rotation;
    gcolor_Color tint;
} gplatform_TextureDrawParams;

void gplatform_begin_drawing(void);

void gplatform_end_drawing(void);

/// Return an invalid texture on bad input, decode failure, or upload failure.
gplatform_Texture gplatform_texture_load(const char* filetype, const void* data, size_t size);

bool gplatform_texture_valid(gplatform_Texture texture);

void gplatform_texture_unload(gplatform_Texture texture);

void gplatform_draw_texture(gplatform_Texture texture, gplatform_TextureDrawParams params);

void gplatform_clear_background(gcolor_Color color);

#ifdef __cplusplus
}
#endif

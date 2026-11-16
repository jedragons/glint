#pragma once

#include <glint/core/alloc.h>
#include <glint/math/color.h>
#include <glint/math/rect.h>
#include <glint/math/vec2.h>
#include <glint/vfs/vfs.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grender_Texture {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} grender_Texture;

// Source and destination are in pixels; origin is the rotation pivot relative
// to the destination's top-left corner.
typedef struct grender_TextureDrawParams {
    grect_Rect source;
    grect_Rect dest;
    gvec2_Vec2 origin;
    float rotation;
    gcolor_Color tint;
} grender_TextureDrawParams;

/// Decode and upload an image. `filetype` is an extension such as ".png".
///
/// # Errors
///
/// Returns an invalid texture on bad input or decode failure.
grender_Texture grender_Texture_load_memory(const char* filetype, const void* data, size_t size);

/// Read and upload an image, inferring its type from the path extension.
///
/// # Errors
///
/// Returns an invalid texture if the image cannot be read or decoded.
grender_Texture grender_Texture_load_vfs(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path);

bool grender_Texture_valid(const grender_Texture* texture);

/// Release the GPU texture and zero the handle. Invalid textures are ignored.
void grender_Texture_unload(grender_Texture* texture);

/// Draw a whole texture at a position with its natural size.
void grender_draw_texture(grender_Texture texture, gvec2_Vec2 position, gcolor_Color tint);

/// Draw a texture region into a destination rectangle with rotation.
void grender_draw_texture_params(grender_Texture texture, grender_TextureDrawParams params);

void grender_clear_background(gcolor_Color color);

#ifdef __cplusplus
}
#endif

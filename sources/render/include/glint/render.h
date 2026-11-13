#ifndef GRENDER_RENDER_H
#define GRENDER_RENDER_H

#include <glint/alloc.h>
#include <glint/types.h>
#include <glint/vfs.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Thin 2D renderer over raylib. Textures are uploaded and drawn through
 * raylib, which controls its own internal allocations; the supplied allocator
 * only covers engine-side work such as reading files from the VFS. */

typedef struct grender_Texture {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} grender_Texture;

/* Source and destination are in pixels; origin is the rotation pivot relative
 * to the destination's top-left corner. */
typedef struct grender_TextureDrawParams {
    grect_Rect source;
    grect_Rect dest;
    gvec2_Vec2 origin;
    float rotation;
    gcolor_Color tint;
} grender_TextureDrawParams;

/* Decode an in-memory image (filetype is an extension such as ".png") and
 * upload it. Returns an invalid texture on bad arguments or decode failure. */
grender_Texture grender_Texture_load_memory(const char *filetype, const void *data, size_t size);
/* Read an image file through the VFS and upload it; the filetype comes from
 * the path extension. Returns an invalid texture when the file is missing or
 * cannot be decoded. */
grender_Texture grender_Texture_load_vfs(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path);
/* True when the texture has GPU data that can be drawn or unloaded. */
bool grender_Texture_valid(const grender_Texture *texture);
/* Releases the GPU texture and resets the handle; unloading an invalid
 * texture is a no-op. */
void grender_Texture_unload(grender_Texture *texture);

/* Draw a whole texture at a position with its natural size. */
void grender_draw_texture(grender_Texture texture, gvec2_Vec2 position, gcolor_Color tint);
/* Draw a texture region into a destination rectangle with rotation. */
void grender_draw_texture_params(grender_Texture texture, grender_TextureDrawParams params);
void grender_clear_background(gcolor_Color color);

#ifdef __cplusplus
}
#endif
#endif // GRENDER_RENDER_H

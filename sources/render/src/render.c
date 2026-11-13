#include <glint/render.h>

#include <limits.h>
#include <raylib.h>
#include <string.h>

/* Glint types never alias raylib types directly; explicit conversions keep
 * the layout differences visible and the raylib header out of glint/render.h. */

static Color grender__render_render_color(gcolor_Color color) {
    return (Color){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a,
    };
}

static Rectangle grender__render_render_rect(grect_Rect rect) {
    return (Rectangle){
        .x = rect.x,
        .y = rect.y,
        .width = rect.w,
        .height = rect.h,
    };
}

static Vector2 grender__render_render_vec2(gvec2_Vec2 vector) {
    return (Vector2){
        .x = vector.x,
        .y = vector.y,
    };
}

static Texture2D grender__render_render_texture(grender_Texture texture) {
    return (Texture2D){
        .id = texture.id,
        .width = texture.width,
        .height = texture.height,
        .mipmaps = texture.mipmaps,
        .format = texture.format,
    };
}

static grender_Texture grender__render_render_from_raylib(Texture2D texture) {
    return (grender_Texture){
        .id = texture.id,
        .width = texture.width,
        .height = texture.height,
        .mipmaps = texture.mipmaps,
        .format = texture.format,
    };
}

/* Copies the path extension including the dot into buffer; returns NULL when
 * the path has no extension. A dot directly after the separator belongs to a
 * hidden file name, not an extension. */
static const char *grender__render_render_filetype(const char *path, char *buffer, size_t capacity) {
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    if (!dot || dot < slash || dot == path || dot[-1] == '/') {
        return NULL;
    }
    size_t length = strlen(dot);
    if (length >= capacity) {
        return NULL;
    }
    memcpy(buffer, dot, length + 1);
    return buffer;
}

grender_Texture grender_Texture_load_memory(const char *filetype, const void *data, size_t size) {
    if (!filetype || !*filetype || !data || !size || size > (size_t)INT_MAX) {
        return (grender_Texture){0};
    }
    Image image = LoadImageFromMemory(filetype, data, (int)size);
    grender_Texture texture = {0};
    if (IsImageValid(image)) {
        texture = grender__render_render_from_raylib(LoadTextureFromImage(image));
        UnloadImage(image);
    }
    return texture;
}

grender_Texture grender_Texture_load_vfs(galloc_Allocator alloc, const gvfs_Vfs *vfs, const char *path) {
    char filetype[16];
    if (!path || !grender__render_render_filetype(path, filetype, sizeof(filetype))) {
        return (grender_Texture){0};
    }
    size_t size = 0;
    void *data = gvfs_read(alloc, vfs, path, &size);
    if (!data) {
        return (grender_Texture){0};
    }
    grender_Texture texture = grender_Texture_load_memory(filetype, data, size);
    galloc_free(alloc, data);
    return texture;
}

bool grender_Texture_valid(const grender_Texture *texture) {
    if (!texture) {
        return false;
    }
    return IsTextureValid(grender__render_render_texture(*texture));
}

void grender_Texture_unload(grender_Texture *texture) {
    if (!texture || texture->id == 0) {
        return;
    }
    UnloadTexture(grender__render_render_texture(*texture));
    *texture = (grender_Texture){0};
}

void grender_draw_texture(grender_Texture texture, gvec2_Vec2 position, gcolor_Color tint) {
    grender_draw_texture_params(
        texture,
        (grender_TextureDrawParams){
            .source = (grect_Rect){.x = 0, .y = 0, .w = (float)texture.width, .h = (float)texture.height},
            .dest = (grect_Rect){.x = position.x, .y = position.y, .w = (float)texture.width, .h = (float)texture.height},
            .origin = (gvec2_Vec2){0},
            .rotation = 0,
            .tint = tint,
        }
    );
}

void grender_draw_texture_params(grender_Texture texture, grender_TextureDrawParams params) {
    DrawTexturePro(
        grender__render_render_texture(texture),
        grender__render_render_rect(params.source),
        grender__render_render_rect(params.dest),
        grender__render_render_vec2(params.origin),
        params.rotation,
        grender__render_render_color(params.tint)
    );
}

void grender_clear_background(gcolor_Color color) {
    ClearBackground(grender__render_render_color(color));
}

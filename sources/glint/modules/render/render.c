#include <glint/modules/render/render.h>
#include <glint/platform/rendering.h>

#include <string.h>

static gplatform_Texture grender__platform_texture(grender_Texture t);
static const char* grender__filetype(const char* path, char* buffer, size_t capacity);

grender_Texture grender_Texture_load_memory(const char* filetype, const void* data, size_t size) {
    gplatform_Texture t = gplatform_texture_load(filetype, data, size);
    return (grender_Texture) {t.id, t.width, t.height, t.mipmaps, t.format};
}

grender_Texture grender_Texture_load_vfs(galloc_Allocator alloc, const gvfs_Vfs* vfs, const char* path) {
    char filetype[16];
    if (path == NULL || grender__filetype(path, filetype, sizeof(filetype)) == NULL) {
        return (grender_Texture) {0};
    }

    size_t size = 0;
    void* data = gvfs_read(alloc, vfs, path, &size);
    if (data == NULL) {
        return (grender_Texture) {0};
    }

    grender_Texture texture = grender_Texture_load_memory(filetype, data, size);
    galloc_free(alloc, data);
    return texture;
}

bool grender_Texture_valid(const grender_Texture* texture) {
    if (texture == NULL) {
        return false;
    }

    return gplatform_texture_valid(grender__platform_texture(*texture));
}

void grender_Texture_unload(grender_Texture* texture) {
    if (texture == NULL || texture->id == 0) {
        return;
    }

    gplatform_texture_unload(grender__platform_texture(*texture));
    *texture = (grender_Texture) {0};
}

void grender_draw_texture(grender_Texture texture, gvec2_Vec2 position, gcolor_Color tint) {
    grender_draw_texture_params(
        texture,
        (grender_TextureDrawParams) {
            .source =
                {
                    .x = 0,
                    .y = 0,
                    .w = (float)texture.width,
                    .h = (float)texture.height,
                },
            .dest =
                {
                    .x = position.x,
                    .y = position.y,
                    .w = (float)texture.width,
                    .h = (float)texture.height,
                },
            .origin = {0},
            .rotation = 0,
            .tint = tint,
        }
    );
}

void grender_draw_texture_params(grender_Texture texture, grender_TextureDrawParams params) {
    gplatform_TextureDrawParams p = {
        .source = {params.source.x, params.source.y, params.source.w, params.source.h},
        .dest = {params.dest.x, params.dest.y, params.dest.w, params.dest.h},
        .origin = {params.origin.x, params.origin.y},
        .rotation = params.rotation,
        .tint = params.tint,
    };
    gplatform_draw_texture(grender__platform_texture(texture), p);
}

void grender_clear_background(gcolor_Color color) {
    gplatform_clear_background(color);
}

static gplatform_Texture grender__platform_texture(grender_Texture t) {
    return (gplatform_Texture) {t.id, t.width, t.height, t.mipmaps, t.format};
}

// A leading dot marks a hidden filename, not an extension.
static const char* grender__filetype(const char* path, char* buffer, size_t capacity) {
    const char* slash = strrchr(path, '/');
    const char* dot = strrchr(path, '.');
    if (dot == NULL || dot == path) {
        return NULL;
    }

    bool dot_in_directory = slash != NULL && dot < slash;
    bool hidden_filename = dot[-1] == '/';
    if (dot_in_directory || hidden_filename) {
        return NULL;
    }

    size_t length = strlen(dot);
    if (length >= capacity) {
        return NULL;
    }

    memcpy(buffer, dot, length + 1);
    return buffer;
}

#include "./rendering.h"
#include "./raylib_internal.h"

#include <limits.h>

static Texture2D gplatform__to_raylib_texture(gplatform_Texture t);
static gplatform_Texture gplatform__from_raylib_texture(Texture2D t);

void gplatform_begin_drawing(void) {
    BeginDrawing();
}

void gplatform_end_drawing(void) {
    EndDrawing();
}

gplatform_Texture gplatform_texture_load(const char* filetype, const void* data, size_t size) {
    graylib_init_logging();
    gplatform_Texture texture = {0};
    Image image = {0};

    if (filetype == NULL || data == NULL) {
        goto cleanup;
    }

    bool size_valid = size > 0 && size <= INT_MAX;
    if (filetype[0] == '\0' || !size_valid) {
        goto cleanup;
    }

    image = LoadImageFromMemory(filetype, data, (int)size);
    if (!IsImageValid(image)) {
        goto cleanup;
    }

    texture = gplatform__from_raylib_texture(LoadTextureFromImage(image));

cleanup:
    if (IsImageValid(image)) {
        UnloadImage(image);
    }
    return texture;
}

bool gplatform_texture_valid(gplatform_Texture texture) {
    return IsTextureValid(gplatform__to_raylib_texture(texture));
}

void gplatform_texture_unload(gplatform_Texture texture) {
    graylib_init_logging();

    UnloadTexture(gplatform__to_raylib_texture(texture));
}

void gplatform_draw_texture(gplatform_Texture texture, gplatform_TextureDrawParams p) {
    DrawTexturePro(
        gplatform__to_raylib_texture(texture),
        (Rectangle) {
            .x = p.source.x,
            .y = p.source.y,
            .width = p.source.w,
            .height = p.source.h,
        },
        (Rectangle) {
            .x = p.dest.x,
            .y = p.dest.y,
            .width = p.dest.w,
            .height = p.dest.h,
        },
        (Vector2) {
            .x = p.origin.x,
            .y = p.origin.y,
        },
        p.rotation,
        (Color) {
            .r = p.tint.r,
            .g = p.tint.g,
            .b = p.tint.b,
            .a = p.tint.a,
        }
    );
}

void gplatform_clear_background(gcolor_Color c) {
    ClearBackground((Color) {
        .r = c.r,
        .g = c.g,
        .b = c.b,
        .a = c.a,
    });
}

static Texture2D gplatform__to_raylib_texture(gplatform_Texture t) {
    return (Texture2D) {
        .id = t.id,
        .width = t.width,
        .height = t.height,
        .mipmaps = t.mipmaps,
        .format = t.format,
    };
}

static gplatform_Texture gplatform__from_raylib_texture(Texture2D t) {
    return (gplatform_Texture) {
        .id = t.id,
        .width = t.width,
        .height = t.height,
        .mipmaps = t.mipmaps,
        .format = t.format,
    };
}

#include <glint/platform/rendering.h>
#include <glint/unit/unit.h>

#include <raylib.h>

#include <limits.h>
#include <string.h>

typedef struct gplatform_render__Context {
    unsigned char pixel;
    Image decoded;
    Texture2D uploaded;
    gplatform_Texture texture;
    size_t decodes;
    size_t uploads;
    size_t image_unloads;
    size_t texture_unloads;
    size_t draws;
    size_t begins;
    size_t ends;
    const void* bytes;
    int size;
    char type[16];
    Texture2D drawn;
    Texture2D unloaded;
    Rectangle source;
    Rectangle dest;
    Vector2 origin;
    float rotation;
    Color tint;
    Color background;
} gplatform_render__Context;

static gplatform_render__Context* gplatform_render__current;

static void gplatform_render__create(gplatform_render__Context* ctx);
static void gplatform_render__free(void* user);
static bool gplatform_render__same_texture(Texture2D actual, gplatform_Texture expected);
static bool gplatform_render__rejects_input(
    gplatform_render__Context* ctx,
    const char* type,
    const void* data,
    size_t size
);

Image LoadImageFromMemory(const char* type, const unsigned char* bytes, int size) {
    gplatform_render__Context* ctx = gplatform_render__current;
    ctx->decodes += 1;
    ctx->bytes = bytes;
    ctx->size = size;
    snprintf(ctx->type, sizeof(ctx->type), "%s", type);
    return ctx->decoded;
}

bool IsImageValid(Image image) {
    return image.data != NULL;
}

Texture2D LoadTextureFromImage(Image image) {
    gplatform_render__Context* ctx = gplatform_render__current;
    if (!gunit_check(
            "upload receives decoded image",
            image.data == ctx->decoded.data && image.width == 19 && image.height == 7
        )) {
        return (Texture2D) {0};
    }
    ctx->uploads += 1;
    return ctx->uploaded;
}

void UnloadImage(Image image) {
    gplatform_render__Context* ctx = gplatform_render__current;
    if (!gunit_check("release decoded image", image.data == ctx->decoded.data)) {
        return;
    }
    ctx->image_unloads += 1;
}

bool IsTextureValid(Texture2D texture) {
    return texture.id != 0;
}

void UnloadTexture(Texture2D texture) {
    gplatform_render__current->unloaded = texture;
    gplatform_render__current->texture_unloads += 1;
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) {
    gplatform_render__Context* ctx = gplatform_render__current;
    ctx->draws += 1;
    ctx->drawn = texture;
    ctx->source = source;
    ctx->dest = dest;
    ctx->origin = origin;
    ctx->rotation = rotation;
    ctx->tint = tint;
}

void ClearBackground(Color color) {
    gplatform_render__current->background = color;
}

void BeginDrawing(void) {
    gplatform_render__current->begins += 1;
}

void EndDrawing(void) {
    gplatform_render__current->ends += 1;
}

void SetTraceLogCallback(TraceLogCallback callback) {
    (void)callback;
}

void SetTraceLogLevel(int level) {
    (void)level;
}

GUNIT_TEST(texture_load,
    gplatform_render__Context ctx = {0};
    const unsigned char bytes[] = {1, 2, 3};

    GUNIT_SETUP({
        gunit_defer(gplatform_render__free, &ctx);
        gplatform_render__create(&ctx);
    })

    GUNIT_SECTION("successful decode and upload preserve texture metadata", {
        ctx.texture = gplatform_texture_load(".png", bytes, sizeof(bytes));

        gunit_assert("decode runs once", ctx.decodes == 1);
        gunit_assert("decode receives original buffer", ctx.bytes == bytes);
        gunit_assert("decode receives full buffer length", ctx.size == (int)sizeof(bytes));
        gunit_assert("decode gets the file type", strcmp(ctx.type, ".png") == 0);
        gunit_assert("upload runs once", ctx.uploads == 1);
        gunit_assert("decoded image is released once", ctx.image_unloads == 1);

        gunit_assert("texture handle is preserved", ctx.texture.id == ctx.uploaded.id);
        gunit_assert("texture width preserved", ctx.texture.width == ctx.uploaded.width);
        gunit_assert("texture height preserved", ctx.texture.height == ctx.uploaded.height);
        gunit_assert("texture mipmaps are preserved", ctx.texture.mipmaps == ctx.uploaded.mipmaps);
        gunit_assert("texture format is preserved", ctx.texture.format == ctx.uploaded.format);
        gunit_assert("backend accepts the returned texture", gplatform_texture_valid(ctx.texture));
    })

    GUNIT_SECTION("decode failure skips the upload", {
        ctx.decoded = (Image) {0};
        ctx.texture = gplatform_texture_load(".png", bytes, sizeof(bytes));

        gunit_assert("decode failure returns an invalid texture", !gplatform_texture_valid(ctx.texture));
        gunit_assert("decode runs once", ctx.decodes == 1);
        gunit_assert("upload is skipped", ctx.uploads == 0);
        gunit_assert("invalid image is left alone", ctx.image_unloads == 0);
    })

    GUNIT_SECTION("upload failure still releases the decoded image", {
        ctx.uploaded = (Texture2D) {0};
        ctx.texture = gplatform_texture_load(".png", bytes, sizeof(bytes));

        gunit_assert("upload failure returns an invalid texture", !gplatform_texture_valid(ctx.texture));
        gunit_assert("upload runs once", ctx.uploads == 1);
        gunit_assert("decoded image is released once", ctx.image_unloads == 1);
    })
)

GUNIT_TEST(texture_load_invalid_input,
    gplatform_render__Context ctx = {0};
    const char bytes[] = "bytes";

    GUNIT_SETUP({
        gunit_defer(gplatform_render__free, &ctx);
        gplatform_render__create(&ctx);
    })

    GUNIT_SECTION("rejects a null file type", {
        gunit_assert("null type is invalid", gplatform_render__rejects_input(&ctx, NULL, bytes, sizeof(bytes) - 1));
    })

    GUNIT_SECTION("rejects an empty file type", {
        gunit_assert("empty type is invalid", gplatform_render__rejects_input(&ctx, "", bytes, sizeof(bytes) - 1));
    })

    GUNIT_SECTION("rejects null bytes", {
        gunit_assert("null bytes are invalid", gplatform_render__rejects_input(&ctx, ".png", NULL, sizeof(bytes) - 1));
    })

    GUNIT_SECTION("rejects zero bytes", {
        gunit_assert("zero bytes are invalid", gplatform_render__rejects_input(&ctx, ".png", bytes, 0));
    })

    GUNIT_SECTION("rejects a size wider than the backend accepts", {
        const size_t oversized = (size_t)INT_MAX + 1;
        gunit_assert("oversized length is invalid", gplatform_render__rejects_input(&ctx, ".png", bytes, oversized));
    })
)

GUNIT_TEST(draw_preserves_texture_and_all_parameters,
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);

    const gplatform_Texture texture = {.id = 41, .width = 19, .height = 7, .mipmaps = 2, .format = 7};
    const gplatform_TextureDrawParams params = {
        .source = {.x = 1, .y = 2, .w = 3, .h = 4},
        .dest = {.x = 5, .y = 6, .w = 7, .h = 8},
        .origin = {.x = 9, .y = 10},
        .rotation = 11,
        .tint = {.r = 12, .g = 13, .b = 14, .a = 15}
    };
    gplatform_draw_texture(texture, params);

    gunit_assert("one draw reaches the backend", ctx.draws == 1);
    gunit_assert("draw forwards texture metadata", gplatform_render__same_texture(ctx.drawn, texture));

    gunit_assert("source rectangle left edge", ctx.source.x == params.source.x);
    gunit_assert("source rectangle top edge", ctx.source.y == params.source.y);
    gunit_assert("source rectangle width", ctx.source.width == params.source.w);
    gunit_assert("source rectangle height", ctx.source.height == params.source.h);

    gunit_assert("destination rectangle left edge", ctx.dest.x == params.dest.x);
    gunit_assert("destination rectangle top edge", ctx.dest.y == params.dest.y);
    gunit_assert("destination rectangle width", ctx.dest.width == params.dest.w);
    gunit_assert("destination rectangle height", ctx.dest.height == params.dest.h);

    gunit_assert("horizontal origin forwarded", ctx.origin.x == params.origin.x);
    gunit_assert("vertical origin forwarded", ctx.origin.y == params.origin.y);
    gunit_assert("rotation forwarded", ctx.rotation == params.rotation);

    gunit_assert("tint red forwarded", ctx.tint.r == params.tint.r);
    gunit_assert("tint green forwarded", ctx.tint.g == params.tint.g);
    gunit_assert("tint blue forwarded", ctx.tint.b == params.tint.b);
    gunit_assert("tint alpha forwarded", ctx.tint.a == params.tint.a);
)

GUNIT_TEST(unload_preserves_backend_handle,
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    const gplatform_Texture texture = {.id = 41, .width = 19, .height = 7, .mipmaps = 2, .format = 7};
    gplatform_texture_unload(texture);

    gunit_assert("one unload reaches the backend", ctx.texture_unloads == 1);
    gunit_assert("unload forwards texture metadata", gplatform_render__same_texture(ctx.unloaded, texture));
)

GUNIT_TEST(frame_and_background_reach_backend,
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    gplatform_begin_drawing();
    gplatform_clear_background((gcolor_Color) {.r = 1, .g = 2, .b = 3, .a = 4});
    gplatform_end_drawing();

    gunit_assert("drawing begins once", ctx.begins == 1);
    gunit_assert("drawing ends once", ctx.ends == 1);
    gunit_assert("background red forwarded", ctx.background.r == 1);
    gunit_assert("background green forwarded", ctx.background.g == 2);
    gunit_assert("background blue forwarded", ctx.background.b == 3);
    gunit_assert("background alpha forwarded", ctx.background.a == 4);
)

gunit_main();

static void gplatform_render__create(gplatform_render__Context* ctx) {
    gplatform_render__current = ctx;
    ctx->decoded = (Image) {.data = &ctx->pixel, .width = 19, .height = 7, .mipmaps = 1, .format = 7};
    ctx->uploaded = (Texture2D) {.id = 11, .width = 19, .height = 7, .mipmaps = 2, .format = 7};
}

static void gplatform_render__free(void* user) {
    gplatform_render__Context* ctx = user;
    if (ctx->texture.id != 0) {
        gplatform_texture_unload(ctx->texture);
    }
    gplatform_render__current = NULL;
}

static bool gplatform_render__rejects_input(
    gplatform_render__Context* ctx,
    const char* type,
    const void* data,
    size_t size
) {
    ctx->texture = gplatform_texture_load(type, data, size);
    if (!gunit_check("invalid input produces no texture", !gplatform_texture_valid(ctx->texture))) {
        return false;
    }
    if (!gunit_check("invalid input skips decoding", ctx->decodes == 0)) {
        return false;
    }
    if (!gunit_check("invalid input skips uploading", ctx->uploads == 0)) {
        return false;
    }
    return gunit_check("invalid input has no image to unload", ctx->image_unloads == 0);
}

static bool gplatform_render__same_texture(Texture2D actual, gplatform_Texture expected) {
    if (!gunit_check("texture id preserved", actual.id == expected.id)) {
        return false;
    }
    if (!gunit_check("texture width preserved", actual.width == expected.width)) {
        return false;
    }
    if (!gunit_check("texture height preserved", actual.height == expected.height)) {
        return false;
    }
    if (!gunit_check("texture mipmaps preserved", actual.mipmaps == expected.mipmaps)) {
        return false;
    }
    return gunit_check("texture format preserved", actual.format == expected.format);
}

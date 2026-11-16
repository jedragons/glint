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

GUNIT_TEST(texture_load_uses_backend_and_preserves_metadata, {
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    const unsigned char bytes[] = {1, 2, 3};
    ctx.texture = gplatform_texture_load(".png", bytes, sizeof(bytes));
    gunit_assert(
        "decode request reaches backend",
        ctx.decodes == 1 && ctx.bytes == bytes && ctx.size == 3 && strcmp(ctx.type, ".png") == 0
    );
    gunit_assert("upload releases decoded data", ctx.uploads == 1 && ctx.image_unloads == 1);
    gunit_assert(
        "metadata crosses adapter",
        ctx.texture.id == 11 && ctx.texture.width == 19 && ctx.texture.height == 7 && ctx.texture.mipmaps == 2
            && ctx.texture.format == 7
    );
    gunit_assert("backend validity preserved", gplatform_texture_valid(ctx.texture));
    gunit_return();
})

#define GPLATFORM_RENDER_INVALID_CASE(name, filetype, bytes, size) \
    GUNIT_TEST(name, { \
        gplatform_render__Context ctx = {0}; \
        gunit_defer(gplatform_render__free, &ctx); \
        gplatform_render__create(&ctx); \
        ctx.texture = gplatform_texture_load(filetype, bytes, size); \
        gunit_assert("invalid input produces no texture", !gplatform_texture_valid(ctx.texture)); \
        gunit_assert( \
            "invalid input does not decode or upload", \
            ctx.decodes == 0 && ctx.uploads == 0 && ctx.image_unloads == 0 \
        ); \
        gunit_return(); \
    })

GPLATFORM_RENDER_INVALID_CASE(null_type, NULL, "bytes", 5)
GPLATFORM_RENDER_INVALID_CASE(empty_type, "", "bytes", 5)
GPLATFORM_RENDER_INVALID_CASE(null_bytes, ".png", NULL, 5)
GPLATFORM_RENDER_INVALID_CASE(empty_bytes, ".png", "bytes", 0)
GPLATFORM_RENDER_INVALID_CASE(oversized_bytes, ".png", "bytes", (size_t)INT_MAX + 1)

GUNIT_TEST(decode_failure_never_uploads, {
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    ctx.decoded = (Image) {0};
    ctx.texture = gplatform_texture_load(".png", "bytes", 5);
    gunit_assert("decode failure produces invalid texture", !gplatform_texture_valid(ctx.texture));
    gunit_assert(
        "no upload or release of invalid image",
        ctx.decodes == 1 && ctx.uploads == 0 && ctx.image_unloads == 0
    );
    gunit_return();
})

GUNIT_TEST(upload_failure_releases_decoded_image, {
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    ctx.uploaded = (Texture2D) {0};
    ctx.texture = gplatform_texture_load(".png", "bytes", 5);
    gunit_assert("upload failure produces invalid texture", !gplatform_texture_valid(ctx.texture));
    gunit_assert("decoded image still released", ctx.uploads == 1 && ctx.image_unloads == 1);
    gunit_return();
})

GUNIT_TEST(draw_preserves_texture_and_all_parameters, {
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    gplatform_Texture texture = {.id = 41, .width = 19, .height = 7, .mipmaps = 2, .format = 7};
    gplatform_TextureDrawParams params =
        {.source = {1, 2, 3, 4}, .dest = {5, 6, 7, 8}, .origin = {9, 10}, .rotation = 11, .tint = {12, 13, 14, 15}};
    gplatform_draw_texture(texture, params);
    gunit_assert(
        "texture metadata forwarded",
        ctx.draws == 1 && ctx.drawn.id == 41 && ctx.drawn.width == 19 && ctx.drawn.height == 7 && ctx.drawn.mipmaps == 2
            && ctx.drawn.format == 7
    );
    gunit_assert(
        "rectangles mapped correctly",
        ctx.source.x == 1 && ctx.source.y == 2 && ctx.source.width == 3 && ctx.source.height == 4 && ctx.dest.x == 5
            && ctx.dest.y == 6 && ctx.dest.width == 7 && ctx.dest.height == 8
    );
    gunit_assert(
        "rotation and tint mapped correctly",
        ctx.origin.x == 9 && ctx.origin.y == 10 && ctx.rotation == 11 && ctx.tint.r == 12 && ctx.tint.g == 13
            && ctx.tint.b == 14 && ctx.tint.a == 15
    );
    gunit_return();
})

GUNIT_TEST(unload_preserves_backend_handle, {
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    gplatform_texture_unload((gplatform_Texture) {.id = 41, .width = 19, .height = 7, .mipmaps = 2, .format = 7});
    gunit_assert(
        "backend releases matching handle",
        ctx.texture_unloads == 1 && ctx.unloaded.id == 41 && ctx.unloaded.width == 19 && ctx.unloaded.height == 7
            && ctx.unloaded.mipmaps == 2 && ctx.unloaded.format == 7
    );
    gunit_return();
})

GUNIT_TEST(frame_and_background_reach_backend, {
    gplatform_render__Context ctx = {0};
    gunit_defer(gplatform_render__free, &ctx);
    gplatform_render__create(&ctx);
    gplatform_begin_drawing();
    gplatform_clear_background((gcolor_Color) {1, 2, 3, 4});
    gplatform_end_drawing();
    gunit_assert("frame boundaries forwarded", ctx.begins == 1 && ctx.ends == 1);
    gunit_assert(
        "background color forwarded",
        ctx.background.r == 1 && ctx.background.g == 2 && ctx.background.b == 3 && ctx.background.a == 4
    );
    gunit_return();
})

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

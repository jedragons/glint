#include <glint/modules/render/render.h>
#include <glint/platform/rendering.h>
#include <glint/unit/unit.h>
#include <glint/vfs/vfs.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct grender__TestContext {
    gplatform_Texture load_result;
    size_t load_count;
    size_t loaded_size;
    char loaded_type[16];
    galloc_Allocator alloc;
    gvfs_Vfs* fs;
    grender_Texture texture;
    FILE* file;
    const char* fixture;
    size_t draw_count;
    size_t unload_count;
    size_t clear_count;
    gcolor_Color cleared_color;
    gplatform_Texture drawn_texture;
    grect_Rect drawn_source;
    grect_Rect drawn_dest;
    gvec2_Vec2 drawn_origin;
    float drawn_rotation;
    gcolor_Color drawn_tint;
} grender__TestContext;

// Graphics stand-ins record into the current test's independent context.
static grender__TestContext* grender__test_context;

static void grender__test_create(grender__TestContext* ctx);
static bool grender__test_load(grender__TestContext* ctx);
static bool grender__test_mount(grender__TestContext* ctx, const char* path);
static void grender__test_free(void* user);

static bool grender__test_color_bytes(
    gcolor_Color color,
    unsigned char r,
    unsigned char g,
    unsigned char b,
    unsigned char a
);

gplatform_Texture gplatform_texture_load(const char* type, const void* bytes, size_t size) {
    grender__TestContext* ctx = grender__test_context;
    (void)bytes;
    ctx->load_count += 1;
    ctx->loaded_size = size;
    snprintf(ctx->loaded_type, sizeof(ctx->loaded_type), "%s", type != NULL ? type : "");
    return ctx->load_result;
}

bool gplatform_texture_valid(gplatform_Texture texture) {
    return texture.id != 0;
}

void gplatform_texture_unload(gplatform_Texture texture) {
    (void)texture;
    grender__test_context->unload_count += 1;
}

void gplatform_draw_texture(gplatform_Texture texture, gplatform_TextureDrawParams params) {
    grender__TestContext* ctx = grender__test_context;
    ctx->drawn_texture = texture;
    ctx->drawn_source = params.source;
    ctx->drawn_dest = params.dest;
    ctx->drawn_origin = params.origin;
    ctx->drawn_rotation = params.rotation;
    ctx->drawn_tint = params.tint;
    ctx->draw_count += 1;
}

void gplatform_clear_background(gcolor_Color color) {
    grender__test_context->cleared_color = color;
    grender__test_context->clear_count += 1;
}

GUNIT_TEST(texture_memory_metadata,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("load fixture", grender__test_load(&ctx));
    gunit_assert("texture dimensions", ctx.texture.id == 7 && ctx.texture.width == 8 && ctx.texture.height == 4);
    gunit_assert("texture metadata", ctx.texture.mipmaps == 1 && ctx.texture.format == 7);
    gunit_assert(
        "memory request forwarded",
        ctx.load_count == 1 && ctx.loaded_size == sizeof("fake png bytes") - 1 && strcmp(ctx.loaded_type, ".png") == 0
    );
    gunit_assert("texture is valid", grender_Texture_valid(&ctx.texture));
)

GUNIT_TEST(texture_unload_resets_handle,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("load fixture", grender__test_load(&ctx));
    grender_Texture_unload(&ctx.texture);
    gunit_assert("texture released", ctx.unload_count == 1);
    gunit_assert("handle reset", ctx.texture.id == 0 && ctx.texture.width == 0);
    gunit_assert("unloaded texture invalid", !grender_Texture_valid(&ctx.texture));
)

GUNIT_TEST(texture_unload_is_idempotent,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("load fixture", grender__test_load(&ctx));
    grender_Texture_unload(&ctx.texture);
    grender_Texture_unload(&ctx.texture);
    gunit_assert("double unload is a no-op", ctx.unload_count == 1);
)

GUNIT_TEST(texture_preserves_platform_failure,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    ctx.load_result = (gplatform_Texture) {0};
    ctx.texture = grender_Texture_load_memory(".png", "bytes", 5);
    gunit_assert("platform failure yields invalid texture", !grender_Texture_valid(&ctx.texture));
)

GUNIT_TEST(texture_vfs_upload,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("mount fixture", grender__test_mount(&ctx, "glint-render-test.png"));
    ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/glint-render-test.png");
    gunit_assert("VFS texture uploaded", ctx.texture.id == 7 && ctx.texture.width == 8 && ctx.texture.height == 4);
    gunit_assert(
        "VFS infers file type and reads bytes",
        ctx.loaded_size == sizeof("fake png bytes") - 1 && strcmp(ctx.loaded_type, ".png") == 0
    );
)

GUNIT_TEST(texture_vfs_rejects_missing,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("mount fixture", grender__test_mount(&ctx, "glint-render-test.png"));
    ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/missing.png");
    gunit_assert("invalid path yields no texture", ctx.texture.id == 0);
)

GUNIT_TEST(texture_vfs_rejects_extensionless,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("mount fixture", grender__test_mount(&ctx, "glint-render-test"));
    ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/glint-render-test");
    gunit_assert("invalid path yields no texture", ctx.texture.id == 0);
    gunit_assert("extensionless file never reaches texture loader", ctx.load_count == 0);
)

GUNIT_TEST(texture_vfs_rejects_hidden,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("mount fixture", grender__test_mount(&ctx, ".png"));
    ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/.png");
    gunit_assert("invalid path yields no texture", ctx.texture.id == 0);
    gunit_assert("hidden file never reaches texture loader", ctx.load_count == 0);
)

GUNIT_TEST(texture_simple_draw_defaults,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("load fixture", grender__test_load(&ctx));
    grender_draw_texture(ctx.texture, (gvec2_Vec2) {.x = 2, .y = 3}, (gcolor_Color) {18, 52, 86, 120});
    gunit_assert("draw reaches backend", ctx.draw_count == 1 && ctx.drawn_texture.id == 7);
    gunit_assert(
        "source covers full texture",
        ctx.drawn_source.x == 0 && ctx.drawn_source.y == 0 && ctx.drawn_source.w == 8 && ctx.drawn_source.h == 4
    );
    gunit_assert(
        "destination uses natural size",
        ctx.drawn_dest.x == 2 && ctx.drawn_dest.y == 3 && ctx.drawn_dest.w == 8 && ctx.drawn_dest.h == 4
    );
    gunit_assert(
        "default pivot and rotation",
        ctx.drawn_origin.x == 0 && ctx.drawn_origin.y == 0 && ctx.drawn_rotation == 0
    );
    gunit_assert(
        "tint forwarded",
        grender__test_color_bytes(
            (gcolor_Color) {ctx.drawn_tint.r, ctx.drawn_tint.g, ctx.drawn_tint.b, ctx.drawn_tint.a},
            18,
            52,
            86,
            120
        )
    );
)

GUNIT_TEST(texture_draw_forwards_params,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    gunit_assert("load fixture", grender__test_load(&ctx));
    grender_draw_texture_params(
        ctx.texture,
        (
            grender_TextureDrawParams
        ) {.source = {.x = 1, .y = 1, .w = 4, .h = 2},
           .dest = {.x = 0, .y = 0, .w = 16, .h = 8},
           .origin = {.x = 4, .y = 2},
           .rotation = 90,
           .tint = GCOLOR_WHITE}
    );
    gunit_assert("params draw reaches backend", ctx.draw_count == 1);
    gunit_assert(
        "source forwarded",
        ctx.drawn_source.x == 1 && ctx.drawn_source.y == 1 && ctx.drawn_source.w == 4 && ctx.drawn_source.h == 2
    );
    gunit_assert("destination forwarded", ctx.drawn_dest.w == 16 && ctx.drawn_dest.h == 8);
    gunit_assert(
        "pivot and rotation forwarded",
        ctx.drawn_origin.x == 4 && ctx.drawn_origin.y == 2 && ctx.drawn_rotation == 90
    );
    gunit_assert(
        "white tint forwarded",
        ctx.drawn_tint.r == 255 && ctx.drawn_tint.g == 255 && ctx.drawn_tint.b == 255 && ctx.drawn_tint.a == 255
    );
)

GUNIT_TEST(clear_black,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    grender_clear_background(GCOLOR_BLACK);
    gunit_assert("clear reaches backend", ctx.clear_count == 1);
    gunit_assert(
        "clear color forwarded",
        ctx.cleared_color.r == 0 && ctx.cleared_color.g == 0 && ctx.cleared_color.b == 0 && ctx.cleared_color.a == 255
    );
)

GUNIT_TEST(clear_transparent,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    grender_clear_background(GCOLOR_BLANK);
    gunit_assert("clear reaches backend", ctx.clear_count == 1);
    gunit_assert(
        "clear color forwarded",
        ctx.cleared_color.r == 0 && ctx.cleared_color.g == 0 && ctx.cleared_color.b == 0 && ctx.cleared_color.a == 0
    );
)
gunit_main();

static bool grender__test_color_bytes(
    gcolor_Color color,
    unsigned char r,
    unsigned char g,
    unsigned char b,
    unsigned char a
) {
    return color.r == r && color.g == g && color.b == b && color.a == a;
}

static void grender__test_create(grender__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    grender__test_context = ctx;
    ctx->load_result = (gplatform_Texture) {.id = 7, .width = 8, .height = 4, .mipmaps = 1, .format = 7};
}

static bool grender__test_load(grender__TestContext* ctx) {
    const char bytes[] = "fake png bytes";
    ctx->texture = grender_Texture_load_memory(".png", bytes, sizeof(bytes) - 1);
    return gunit_check("upload fixture texture", grender_Texture_valid(&ctx->texture));
}

static bool grender__test_mount(grender__TestContext* ctx, const char* path) {
    ctx->file = fopen(path, "wb");
    if (!gunit_check("open image fixture", ctx->file != NULL)) {
        return false;
    }
    ctx->fixture = path;
    const char bytes[] = "fake png bytes";
    if (!gunit_check("write image fixture", fwrite(bytes, 1, sizeof(bytes) - 1, ctx->file) == sizeof(bytes) - 1)) {
        return false;
    }
    int closed = fclose(ctx->file);
    ctx->file = NULL;
    if (!gunit_check("close image fixture", closed == 0)) {
        return false;
    }
    ctx->fs = gvfs_create(ctx->alloc);
    if (!gunit_check("create VFS", ctx->fs != NULL)) {
        return false;
    }
    return gunit_check("mount VFS", gvfs_mount(ctx->alloc, ctx->fs, "/", "."));
}

static void grender__test_free(void* user) {
    grender__TestContext* ctx = user;
    grender_Texture_unload(&ctx->texture);
    if (ctx->fs != NULL) {
        gvfs_destroy(ctx->alloc, ctx->fs);
    }
    if (ctx->file != NULL) {
        fclose(ctx->file);
    }
    if (ctx->fixture != NULL) {
        remove(ctx->fixture);
    }
    grender__test_context = NULL;
}

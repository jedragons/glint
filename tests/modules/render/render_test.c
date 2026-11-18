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
static bool grender__test_write_image(grender__TestContext* ctx, const char* path);
static void grender__test_free(void* user);

static bool grender__test_same_color(gcolor_Color actual, gcolor_Color expected);
static bool grender__test_same_rect(grect_Rect actual, grect_Rect expected);

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

GUNIT_TEST(texture_loaded_from_memory,
    grender__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(grender__test_free, &ctx);
        grender__test_create(&ctx);
        gunit_assert("upload fixture texture", grender__test_load(&ctx));
    })

    GUNIT_SECTION("upload forwards the request and retains the platform metadata", {
        gunit_assert("one upload", ctx.load_count == 1);
        gunit_assert("file type forwarded", strcmp(ctx.loaded_type, ".png") == 0);
        gunit_assert("byte count forwarded", ctx.loaded_size == sizeof("fake png bytes") - 1);

        gunit_assert("texture id", ctx.texture.id == 7);
        gunit_assert("texture dimensions", ctx.texture.width == 8 && ctx.texture.height == 4);
        gunit_assert("texture metadata", ctx.texture.mipmaps == 1 && ctx.texture.format == 7);
        gunit_assert("texture is valid", grender_Texture_valid(&ctx.texture));
    })

    GUNIT_SECTION("unload clears the handle and releases the texture only once", {
        grender_Texture_unload(&ctx.texture);

        gunit_assert("texture released", ctx.unload_count == 1);
        gunit_assert("handle reset", ctx.texture.id == 0 && ctx.texture.width == 0);
        gunit_assert("unloaded texture invalid", !grender_Texture_valid(&ctx.texture));

        grender_Texture_unload(&ctx.texture);
        gunit_assert("second unload is a no-op", ctx.unload_count == 1);
    })
)

GUNIT_TEST(texture_preserves_platform_failure,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);
    ctx.load_result = (gplatform_Texture) {0};

    ctx.texture = grender_Texture_load_memory(".png", "bytes", 5);
    gunit_assert("platform failure yields invalid texture", !grender_Texture_valid(&ctx.texture));
)

GUNIT_TEST(texture_loaded_from_vfs,
    grender__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(grender__test_free, &ctx);
        grender__test_create(&ctx);
        ctx.fs = gvfs_create(ctx.alloc);
        gunit_assert("create VFS", ctx.fs != NULL);
        gunit_assert("mount fixture directory", gvfs_mount(ctx.alloc, ctx.fs, "/", "."));
    })

    GUNIT_SECTION("a PNG is uploaded with its inferred file type", {
        gunit_assert("write PNG fixture", grender__test_write_image(&ctx, "glint-render-test.png"));
        ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/glint-render-test.png");

        gunit_assert("texture uploaded", ctx.texture.id == 7 && ctx.texture.width == 8 && ctx.texture.height == 4);
        gunit_assert("file type inferred from the extension", strcmp(ctx.loaded_type, ".png") == 0);
        gunit_assert("file bytes read", ctx.loaded_size == sizeof("fake png bytes") - 1);
    })

    GUNIT_SECTION("a missing file yields no texture", {
        ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/missing.png");
        gunit_assert("missing file yields no texture", ctx.texture.id == 0);
    })

    GUNIT_SECTION("a file without an extension never reaches the texture loader", {
        gunit_assert("write extensionless fixture", grender__test_write_image(&ctx, "glint-render-test"));
        ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/glint-render-test");

        gunit_assert("extensionless file yields no texture", ctx.texture.id == 0);
        gunit_assert("no upload attempted", ctx.load_count == 0);
    })

    GUNIT_SECTION("a leading dot alone does not supply a file extension", {
        gunit_assert("write hidden fixture", grender__test_write_image(&ctx, ".png"));
        ctx.texture = grender_Texture_load_vfs(ctx.alloc, ctx.fs, "/.png");

        gunit_assert("hidden file yields no texture", ctx.texture.id == 0);
        gunit_assert("no upload attempted", ctx.load_count == 0);
    })
)

GUNIT_TEST(texture_draw,
    grender__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(grender__test_free, &ctx);
        grender__test_create(&ctx);
        gunit_assert("upload fixture texture", grender__test_load(&ctx));
    })

    GUNIT_SECTION("simple draw uses natural size at the requested position", {
        const gcolor_Color tint = {.r = 18, .g = 52, .b = 86, .a = 120};
        grender_draw_texture(ctx.texture, (gvec2_Vec2) {.x = 2, .y = 3}, tint);

        gunit_assert("draw reaches backend", ctx.draw_count == 1 && ctx.drawn_texture.id == 7);
        const grect_Rect full_texture = {.w = 8, .h = 4};
        gunit_assert("source covers full texture", grender__test_same_rect(ctx.drawn_source, full_texture));
        gunit_assert("natural size at (2, 3)", grender__test_same_rect(ctx.drawn_dest, (grect_Rect) {2, 3, 8, 4}));
        gunit_assert("default origin", ctx.drawn_origin.x == 0 && ctx.drawn_origin.y == 0);
        gunit_assert("default rotation", ctx.drawn_rotation == 0);
        gunit_assert("tint forwarded", grender__test_same_color(ctx.drawn_tint, tint));
    })

    GUNIT_SECTION("explicit draw parameters reach the backend", {
        const grender_TextureDrawParams params = {
            .source = {.x = 1, .y = 1, .w = 4, .h = 2},
            .dest = {.x = 0, .y = 0, .w = 16, .h = 8},
            .origin = {.x = 4, .y = 2},
            .rotation = 90,
            .tint = GCOLOR_WHITE,
        };

        grender_draw_texture_params(ctx.texture, params);

        gunit_assert("one draw", ctx.draw_count == 1);
        gunit_assert("source forwarded", grender__test_same_rect(ctx.drawn_source, params.source));
        gunit_assert("destination forwarded", grender__test_same_rect(ctx.drawn_dest, params.dest));
        gunit_assert(
            "origin forwarded", ctx.drawn_origin.x == params.origin.x && ctx.drawn_origin.y == params.origin.y
        );
        gunit_assert("rotation forwarded", ctx.drawn_rotation == params.rotation);
        const gcolor_Color white = {255, 255, 255, 255};
        gunit_assert("white tint forwarded", grender__test_same_color(ctx.drawn_tint, white));
    })
)

GUNIT_TEST(clear_background,
    grender__TestContext ctx = {0};
    gunit_defer(grender__test_free, &ctx);
    grender__test_create(&ctx);

    grender_clear_background(GCOLOR_BLACK);
    gunit_assert("first clear reaches backend", ctx.clear_count == 1);
    gunit_assert("opaque black forwarded", grender__test_same_color(ctx.cleared_color, (gcolor_Color) {0, 0, 0, 255}));

    grender_clear_background(GCOLOR_BLANK);
    gunit_assert("second clear reaches backend", ctx.clear_count == 2);
    gunit_assert("transparent black forwarded", grender__test_same_color(ctx.cleared_color, (gcolor_Color) {0}));
)

gunit_main();

static bool grender__test_same_color(gcolor_Color actual, gcolor_Color expected) {
    return actual.r == expected.r && actual.g == expected.g && actual.b == expected.b && actual.a == expected.a;
}

static bool grender__test_same_rect(grect_Rect actual, grect_Rect expected) {
    return actual.x == expected.x && actual.y == expected.y && actual.w == expected.w && actual.h == expected.h;
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

static bool grender__test_write_image(grender__TestContext* ctx, const char* path) {
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
    return gunit_check("close image fixture", closed == 0);
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

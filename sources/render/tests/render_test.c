#include <glint/render.h>
#include <glint/vfs.h>

#include <mewunit.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* Stand-in graphics functions keep CI headless; no real GPU is touched. */
static unsigned grender__test_draw_count, grender__test_unload_count, grender__test_clear_count;
static Color grender__test_cleared_color;
static Texture2D grender__test_drawn_texture;
static Rectangle grender__test_drawn_source, grender__test_drawn_dest;
static Vector2 grender__test_drawn_origin;
static float grender__test_drawn_rotation;
static Color grender__test_drawn_tint;

Image LoadImageFromMemory(const char *type, const unsigned char *bytes, int size) {
    if (strcmp(type, ".png") != 0) {
        return (Image){0};
    }
    mewrequire("Test expectation failed", bytes && size > 0);
    return (Image){.data = (void *)bytes, .width = 8, .height = 4, .mipmaps = 1, .format = 7};
}

bool IsImageValid(Image image) {
    return image.data != NULL;
}

void UnloadImage(Image image) {
    (void)image;
}

Texture2D LoadTextureFromImage(Image image) {
    mewrequire("Test expectation failed", image.data);
    return (Texture2D){.id = 7, .width = 8, .height = 4, .mipmaps = 1, .format = 7};
}

bool IsTextureValid(Texture2D texture) {
    return texture.id != 0 && texture.width > 0 && texture.height > 0;
}

void UnloadTexture(Texture2D texture) {
    mewrequire("Test expectation failed", texture.id != 0);
    grender__test_unload_count++;
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) {
    mewrequire("Test expectation failed", texture.id != 0 && grender__test_draw_count < 16);
    grender__test_drawn_texture = texture;
    grender__test_drawn_source = source;
    grender__test_drawn_dest = dest;
    grender__test_drawn_origin = origin;
    grender__test_drawn_rotation = rotation;
    grender__test_drawn_tint = tint;
    grender__test_draw_count++;
}

void ClearBackground(Color color) {
    mewrequire("Test expectation failed", grender__test_clear_count < 16);
    grender__test_cleared_color = color;
    grender__test_clear_count++;
}

static bool grender__test_color_bytes(gcolor_Color color, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return color.r == r && color.g == g && color.b == b && color.a == a;
}

TEST(texture_memory, {
    const char bytes[] = "fake png bytes";
    grender_Texture texture = grender_Texture_load_memory(".png", bytes, sizeof(bytes) - 1);
    mewassert("texture uploaded", texture.id == 7 && texture.width == 8 && texture.height == 4);
    mewassert("texture metadata", texture.mipmaps == 1 && texture.format == 7);
    mewassert("texture is valid", grender_Texture_valid(&texture));

    unsigned before = grender__test_unload_count;
    grender_Texture_unload(&texture);
    mewassert("texture released", grender__test_unload_count == before + 1);
    mewassert("handle reset", texture.id == 0 && texture.width == 0);
    mewassert("handle is invalid after unload", !grender_Texture_valid(&texture));
    grender_Texture_unload(&texture);
    mewassert("double unload is a no-op", grender__test_unload_count == before + 1);
})

TEST(texture_memory_invalid, {
    const char bytes[] = "fake png bytes";
    mewassert("null data rejected", grender_Texture_load_memory(".png", NULL, 8).id == 0);
    mewassert("empty data rejected", grender_Texture_load_memory(".png", bytes, 0).id == 0);
    mewassert("missing filetype rejected", grender_Texture_load_memory("", bytes, 8).id == 0);
    mewassert("null filetype rejected", grender_Texture_load_memory(NULL, bytes, 8).id == 0);
    grender_Texture texture = grender_Texture_load_memory(".qoi", bytes, sizeof(bytes) - 1);
    mewassert("undecodable data yields an invalid texture", texture.id == 0 && !grender_Texture_valid(&texture));
    mewassert("invalid handles may be unloaded safely", (grender_Texture_unload(&texture), true));
})

TEST(texture_vfs, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    const char bytes[] = "fake png bytes";
    FILE *file = fopen("glint-render-test.png", "wb");
    mewassert("Test expectation failed", file);
    mewassert("Test expectation failed", fwrite(bytes, 1, sizeof(bytes) - 1, file) == sizeof(bytes) - 1);
    mewassert("Test expectation failed", fclose(file) == 0);

    gvfs_Vfs *fs = gvfs_create(alloc);
    mewassert("Test expectation failed", fs && gvfs_mount(alloc, fs, "/", "."));

    grender_Texture texture = grender_Texture_load_vfs(alloc, fs, "/glint-render-test.png");
    mewassert("vfs texture uploaded", texture.id == 7 && texture.width == 8 && texture.height == 4);
    grender_Texture_unload(&texture);

    mewassert("missing files yield invalid textures", grender_Texture_load_vfs(alloc, fs, "/missing.png").id == 0);
    mewassert("extensionless paths yield invalid textures", grender_Texture_load_vfs(alloc, fs, "/glint-render-test").id == 0);
    mewassert("hidden paths are not extensions", grender_Texture_load_vfs(alloc, fs, "/.png").id == 0);
    gvfs_destroy(alloc, fs);
})

TEST(texture_draw, {
    const char bytes[] = "fake png bytes";
    grender_Texture texture = grender_Texture_load_memory(".png", bytes, sizeof(bytes) - 1);

    grender_draw_texture(texture, (gvec2_Vec2){.x = 2.0f, .y = 3.0f}, (gcolor_Color){18, 52, 86, 120});
    mewassert("draw reached the backend", grender__test_draw_count == 1 && grender__test_drawn_texture.id == 7);
    mewassert("simple draw sources the whole texture",
              grender__test_drawn_source.x == 0 && grender__test_drawn_source.y == 0 && grender__test_drawn_source.width == 8 && grender__test_drawn_source.height == 4);
    mewassert("simple draw keeps the natural size",
              grender__test_drawn_dest.x == 2 && grender__test_drawn_dest.y == 3 && grender__test_drawn_dest.width == 8 && grender__test_drawn_dest.height == 4);
    mewassert("simple draw has no pivot or rotation", grender__test_drawn_origin.x == 0 && grender__test_drawn_origin.y == 0 && grender__test_drawn_rotation == 0);
    mewassert("tint reached the backend", grender__test_color_bytes((gcolor_Color){grender__test_drawn_tint.r, grender__test_drawn_tint.g, grender__test_drawn_tint.b, grender__test_drawn_tint.a}, 18, 52, 86, 120));

    grender_draw_texture_params(
        texture,
        (grender_TextureDrawParams){
            .source = (grect_Rect){.x = 1, .y = 1, .w = 4, .h = 2},
            .dest = (grect_Rect){.x = 0, .y = 0, .w = 16, .h = 8},
            .origin = (gvec2_Vec2){.x = 4, .y = 2},
            .rotation = 90,
            .tint = GCOLOR_WHITE,
        }
    );
    mewassert("params draw reached the backend", grender__test_draw_count == 2);
    mewassert("source rect forwarded",
              grender__test_drawn_source.x == 1 && grender__test_drawn_source.y == 1 && grender__test_drawn_source.width == 4 && grender__test_drawn_source.height == 2);
    mewassert("dest rect forwarded", grender__test_drawn_dest.width == 16 && grender__test_drawn_dest.height == 8);
    mewassert("origin and rotation forwarded", grender__test_drawn_origin.x == 4 && grender__test_drawn_origin.y == 2 && grender__test_drawn_rotation == 90);
    mewassert("white tint forwarded", grender__test_drawn_tint.r == 255 && grender__test_drawn_tint.g == 255 && grender__test_drawn_tint.b == 255 && grender__test_drawn_tint.a == 255);

    grender_clear_background(GCOLOR_BLACK);
    mewassert("clears reached the backend", grender__test_clear_count == 1);
    mewassert("black clear forwarded", grender__test_cleared_color.r == 0 && grender__test_cleared_color.g == 0 && grender__test_cleared_color.b == 0 && grender__test_cleared_color.a == 255);
    grender_clear_background(GCOLOR_BLANK);
    mewassert("transparent clear forwarded", grender__test_cleared_color.r == 0 && grender__test_cleared_color.g == 0 && grender__test_cleared_color.b == 0 && grender__test_cleared_color.a == 0);
    grender_Texture_unload(&texture);
})

mewtest_main();

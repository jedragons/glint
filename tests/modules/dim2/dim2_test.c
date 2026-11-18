#include <glint/modules/assets/module.h>
#include <glint/modules/dim2/bindings.h>
#include <glint/modules/dim2/dim2.h>
#include <glint/modules/dim2/module.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/module.h>
#include <glint/modules/script/script.h>
#include <glint/modules/texture/texture_asset.h>
#include <glint/modules/time/time.h>
#include <glint/modules/types/bindings.h>
#include <glint/modules/types/module.h>
#include <glint/modules/window/bindings.h>
#include <glint/modules/window/module.h>
#include <glint/platform/rendering.h>
#include <glint/runtime/config.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct gdim2__TestUnload {
    gmod_Modules* modules;
    gdim2_Dim2* dim2;
} gdim2__TestUnload;

typedef struct gdim2__TestContext {
    galloc_Allocator alloc;
    gecs_World* world;
    gvfs_Vfs* vfs;
    gscript_lua_Runtime* script;
    gasset_Cache* cache;
    gdim2_Dim2* dim2;
    gtexture_Assets* textures;
    gmod_Modules* modules;
    gecs_World* other_world;
    gdim2_Dim2* other_dim2;
    char* source;
    int module;
    bool owns_script;
    bool owns_dim2;
    bool shared;
    bool native_fixtures;
    bool texture_fixture;
    size_t draws;
    size_t unloads;
    grect_Rect sources[16];
    grect_Rect destinations[16];
    gdim2__TestUnload callback;
    gasset_Service host_assets;
    glint_Config config;
    gtime_FrameTime clock;
    gwindow_Host window;
    gtypes_LuaService lua_service;
} gdim2__TestContext;

static gdim2__TestContext* gdim2__test_active;

// Stand-in graphics functions allow exercising asset loading and rendering in
// CI without opening a window; the PNG bytes still come from the actual VFS.
static int gdim2__test_spawn(lua_State* lua);
static bool gdim2__test_write_asset(const char* path, const char* data);
static bool gdim2__test_run_script(
    galloc_Allocator alloc,
    gscript_lua_Runtime* script,
    const char* source,
    const char* name
);
static bool gdim2__test_virtual_exists(galloc_Allocator alloc, void* context, const char* path);
static void* gdim2__test_virtual_read(galloc_Allocator alloc, void* context, const char* path, size_t* size);
static bool gdim2__test_virtual_list(
    galloc_Allocator alloc,
    void* context,
    const char* path,
    gvfs_EntryFn visit,
    void* user
);
static bool gdim2__test_unload_during_load(galloc_Allocator alloc, void* instance, const char* path, void* user);

static void gdim2__test_create(gdim2__TestContext* ctx);
static bool gdim2__test_create_bindings(gdim2__TestContext* ctx, bool shared);
static bool gdim2__test_create_reload(gdim2__TestContext* ctx, bool shared);
static bool gdim2__test_create_virtual(gdim2__TestContext* ctx);
static bool gdim2__test_create_texture(gdim2__TestContext* ctx);
static bool gdim2__test_create_module(gdim2__TestContext* ctx);
static bool gdim2__test_load_example(gdim2__TestContext* ctx);
static bool gdim2__test_create_other_world(gdim2__TestContext* ctx);
static bool gdim2__test_rebind(gdim2__TestContext* ctx);
static void gdim2__test_update(gdim2__TestContext* ctx);
static void gdim2__test_destroy(void* user);

gplatform_Texture gplatform_texture_load(const char* type, const void* bytes, size_t size) {
    if (!gunit_check(
            "texture loader receives PNG bytes",
            type != NULL && strcmp(type, ".png") == 0 && bytes != NULL && size > 0
        )) {
        return (gplatform_Texture) {0};
    }
    return (gplatform_Texture) {.id = 1, .width = 64, .height = 16, .mipmaps = 1, .format = 7};
}

bool gplatform_texture_valid(gplatform_Texture texture) {
    return texture.id != 0;
}

void gplatform_texture_unload(gplatform_Texture texture) {
    if (!gunit_check("unload valid texture", texture.id != 0)) {
        return;
    }
    gdim2__test_active->unloads += 1;
}

void gplatform_draw_texture(gplatform_Texture texture, gplatform_TextureDrawParams params) {
    gdim2__TestContext* ctx = gdim2__test_active;
    if (!gunit_check(
            "drawing uses expected defaults",
            texture.id != 0 && params.origin.x == 0 && params.origin.y == 0 && params.rotation == 0
                && params.tint.a == 255
        )) {
        return;
    }
    if (!gunit_check("draw recording has capacity", ctx->draws < 16)) {
        return;
    }
    ctx->sources[ctx->draws] = params.source;
    ctx->destinations[ctx->draws] = params.dest;
    ctx->draws += 1;
}

void gplatform_clear_background(gcolor_Color color) {
    (void)color;
}

GUNIT_TEST(virtual_atlas_without_native_path,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("create virtual", gdim2__test_create_virtual(&ctx));

    const char* source = "assert(Atlas.load('/atlas.json'):sprite('f'))";
    gunit_assert(
        "atlas loads without native paths",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "virtual-atlas")
    );
)

GUNIT_TEST(shared_texture,
    gdim2__TestContext ctx = {0};
    gtexture_Asset* texture = NULL;

    GUNIT_SETUP({
        gunit_defer(gdim2__test_destroy, &ctx);
        gunit_assert("create shared texture registry", gdim2__test_create_texture(&ctx));
        texture = gtexture_load(ctx.alloc, ctx.textures, "/glint-shared-texture.png");
        gunit_assert("load initial texture", texture != NULL);
    })

    GUNIT_SECTION("repeated loads reuse the original asset", {
        gtexture_Asset* loaded = gtexture_load(ctx.alloc, ctx.textures, "/glint-shared-texture.png");
        gunit_assert("same path shares one asset", loaded == texture);
    })

    GUNIT_SECTION("reload replaces the image and preserves the asset handle", {
        const size_t previous_unloads = ctx.unloads;
        gunit_assert(
            "write replacement texture",
            gdim2__test_write_asset("glint-shared-texture.png", "replacement image with different size")
        );

        gasset_Cache_poll(ctx.alloc, ctx.cache);

        gtexture_Asset* reloaded = gtexture_load(ctx.alloc, ctx.textures, "/glint-shared-texture.png");
        gunit_assert("reload preserves shared handle", reloaded == texture);
        gunit_assert("reload unloads the previous image once", ctx.unloads == previous_unloads + 1);
    })
)

GUNIT_TEST(dim2_duplicate_dependency_imports,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("create the 2D module", gdim2__test_create_module(&ctx));

    gmod_Module descriptor = gdim2_module();
    gmod_Module types = gtypes_module();
    gunit_assert("dim2 can be imported again", gmod_Modules_import(ctx.alloc, ctx.modules, descriptor));
    gunit_assert("types can be imported again", gmod_Modules_import(ctx.alloc, ctx.modules, types));
    gunit_assert("load imported modules", gmod_Modules_load(ctx.alloc, ctx.modules));
    gdim2_Service* service = gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gdim2_Service));
    gunit_assert("2D instance published", service != NULL && service->instance != NULL);
)

GUNIT_TEST(dim2_example_sprite_animation,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("create 2D module", gdim2__test_create_module(&ctx));
    gunit_assert("load example game", gdim2__test_load_example(&ctx));

    gmod_Modules_render(ctx.alloc, ctx.modules);
    gunit_assert("draws both sprites", ctx.draws == 2);
    gunit_assert("draws sprites at their positions", ctx.destinations[0].x == 96 && ctx.destinations[1].x == 224);
    gunit_assert("draws the first frames", ctx.sources[0].x == 0 && ctx.sources[1].x == 0);

    gmod_Modules_update(ctx.alloc, ctx.modules);
    gmod_Modules_render(ctx.alloc, ctx.modules);
    gunit_assert("draws both sprites again", ctx.draws == 4);
    gunit_assert("draws the next animation frame", ctx.sources[3].x == 16);
)

GUNIT_TEST(dim2_module_unload_removes_service_and_texture,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("create the 2D module", gdim2__test_create_module(&ctx));
    gunit_assert("load example", gdim2__test_load_example(&ctx));

    gscript_lua_release_module(ctx.alloc, ctx.script, ctx.module);
    ctx.module = LUA_NOREF;
    gunit_assert("module unload succeeds", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert("2D service removed", gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gdim2_Service)) == NULL);
    gunit_assert("atlas texture unloaded", ctx.unloads == 1);
)

GUNIT_TEST(dim2_unload_during_cache_callback,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("create the 2D module", gdim2__test_create_module(&ctx));

    ctx.cache = gasset_Cache_create(ctx.alloc);
    ctx.host_assets.cache = ctx.cache;
    gunit_assert(
        "host cache created",
        ctx.cache != NULL
            && gecs_context_set(ctx.world, GECS_CONTEXT(ctx.world, gasset_Service), &ctx.host_assets) != NULL
    );
    gunit_assert("load modules", gmod_Modules_load(ctx.alloc, ctx.modules));
    gecs_Component type = GECS_CONTEXT(ctx.world, gdim2_Service);
    gdim2_Service* service = gecs_context_get(ctx.world, type);
    gunit_assert("2D instance published", service != NULL && service->instance != NULL);
    ctx.callback = (gdim2__TestUnload) {.modules = ctx.modules, .dim2 = service->instance};
    gunit_assert(
        "register unload callback",
        gasset_Cache_register(
            ctx.alloc,
            ctx.cache,
            "unloader",
            sizeof(int),
            gdim2__test_unload_during_load,
            NULL,
            NULL,
            &ctx.callback
        )
    );
    gunit_assert("unload inside callback", gasset_Cache_load(ctx.alloc, ctx.cache, "unloader", "/item") != NULL);
    service = gecs_context_get(ctx.world, type);
    gunit_assert("service retained until retry", service != NULL && service->instance != NULL);
    gunit_assert("unload succeeds after callback", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert("service context removed", gecs_context_get(ctx.world, type) == NULL);
)

GUNIT_TEST(dim2_missing_host_services,
    gdim2__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gdim2__test_destroy, &ctx);
        gdim2__test_create(&ctx);

        ctx.world = gecs_World_new(ctx.alloc);
        gunit_assert("create the module world", ctx.world != NULL);
        ctx.modules = gmod_Modules_new(ctx.world);
        gunit_assert("create the module registry", ctx.modules != NULL);
    })

    GUNIT_SECTION("fails dependency loading without a Lua service", {
        gunit_assert("import without a Lua service", gmod_Modules_import(ctx.alloc, ctx.modules, gdim2_module()));
        gunit_assert("missing Lua service fails dependency load", !gmod_Modules_load(ctx.alloc, ctx.modules));
    })

    GUNIT_SECTION("fails without a window host and leaves no dim2 instance", {
        ctx.script = gscript_lua_new(ctx.alloc);
        ctx.owns_script = true;
        gunit_assert("create the Lua runtime", ctx.script != NULL);
        ctx.lua_service.lua = gscript_lua_state(ctx.script);
        gunit_assert(
            "install the Lua service",
            gecs_context_set(ctx.world, GECS_CONTEXT(ctx.world, gtypes_LuaService), &ctx.lua_service) != NULL
        );

        gunit_assert("import with the host Lua service", gmod_Modules_import(ctx.alloc, ctx.modules, gdim2_module()));
        gunit_assert("missing host fails module load", !gmod_Modules_load(ctx.alloc, ctx.modules));
        gecs_Component service = GECS_CONTEXT(ctx.world, gdim2_Service);
        gunit_assert("no partial instance", gecs_context_get(ctx.world, service) == NULL);
    })
)

// Run each scenario once with an instance-owned cache and once with a host cache.
#define GDIM2_TEST_WITH_CACHE(name, shared, ...) \
    GUNIT_TEST(name, \
        gdim2__TestContext ctx = {0}; \
        gunit_defer(gdim2__test_destroy, &ctx); \
        gunit_assert("load the sprite fixture", gdim2__test_create_reload(&ctx, shared)); \
        __VA_ARGS__; \
    )

#define GDIM2_TEST_WITH_CACHES(name, ...) \
    GDIM2_TEST_WITH_CACHE(name##_local, false, __VA_ARGS__) \
    GDIM2_TEST_WITH_CACHE(name##_shared, true, __VA_ARGS__)

GUNIT_TEST(dim2_transform_mutation_invalidates_removed_handles,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("bind the 2D API", gdim2__test_create_bindings(&ctx, false));

    const char* source =
        "saved = Transform2D.get(entity)\n"
        "saved.position.x = 13\n"
        "saved.position.y = 17\n"
        "saved.scale = Vec2.new(2, 3)\n"
        "saved.origin = Vec2.new()\n"
        "saved.rotation = 0\n"
        "assert(Transform2D.get(entity).position.x == 13 and saved.scale.y == 3)\n"
        "Transform2D.set(entity, saved)\n"
        "vector = saved.position\n"
        "Transform2D.remove(entity)\n"
        "assert(not pcall(function() return saved.rotation end))\n"
        "assert(not pcall(function() return vector.x end))\n"
        "Transform2D.set(entity, Vec2.new(13, 17), Vec2.new(2, 3), Vec2.new(), 0)\n";
    gunit_assert(
        "removed transform and vector handles reject reads",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "transform_mutation")
    );
)

GDIM2_TEST_WITH_CACHES(dim2_transform_controls_rendered_destination, {
    gdim2_render(ctx.dim2);
    gunit_assert("both sprites are drawn", ctx.draws == 2);
    gunit_assert("sprites use the original frame", ctx.sources[0].x == 1 && ctx.sources[1].x == 1);
    grect_Rect destination = ctx.destinations[0];
    gunit_assert("transform places the sprite", destination.x == 13 && destination.y == 17);
    gunit_assert("transform scales the sprite", destination.w == 8 && destination.h == 15);
})

GDIM2_TEST_WITH_CACHES(dim2_invalid_atlas_reload_preserves_live_frames, {
    gunit_assert("write asset", gdim2__test_write_asset("glint-dim2-test-atlas.json", "invalid JSON"));
    gdim2__test_update(&ctx);
    gdim2_render(ctx.dim2);
    gunit_assert("invalid reload preserves frames", ctx.draws == 2 && ctx.sources[0].x == 1 && ctx.sources[1].x == 1);
})

GDIM2_TEST_WITH_CACHES(dim2_missing_frame_reload_preserves_live_frames, {
    gunit_assert(
        "write asset",
        gdim2__test_write_asset("glint-dim2-test-atlas.json", "{\"frames\":{\"other\":{}},\"animations\":{}}")
    );
    gdim2__test_update(&ctx);
    gdim2_render(ctx.dim2);
    gunit_assert(
        "missing name preserves live frames",
        ctx.draws == 2 && ctx.sources[0].x == 1 && ctx.sources[1].x == 1
    );
})

GDIM2_TEST_WITH_CACHES(dim2_failed_reload_retries_after_live_users_removed, {
    gunit_assert(
        "write asset",
        gdim2__test_write_asset("glint-dim2-test-atlas.json", "{\"frames\":{\"other\":{}},\"animations\":{}}")
    );
    gdim2__test_update(&ctx);
    gunit_assert(
        "remove the sprites that reference the old frame",
        gdim2__test_run_script(
            ctx.alloc,
            ctx.script,
            "Sprite.remove(entity)\n"
            "AnimatedSprite.remove(entity)\n",
            "remove_live_users"
        )
    );
    gdim2_update(ctx.alloc, ctx.dim2, 0);
    gunit_assert(
        "reload succeeds without another file change",
        gdim2__test_run_script(
            ctx.alloc,
            ctx.script,
            "assert(Atlas.load('/glint-dim2-test-atlas.json'):sprite('other'))",
            "retry_without_file_change"
        )
    );
})

GDIM2_TEST_WITH_CACHES(dim2_valid_atlas_reload_updates_live_frames, {
    gunit_assert(
        "write asset",
        gdim2__test_write_asset(
            "glint-dim2-test-atlas.json",
            "{\"frames\":{\"f\":{\"x\":7,\"w\":9,\"h\":5}},\"animations\":{\"a\":[\"f\"]}}"
        )
    );
    gdim2__test_update(&ctx);
    gdim2_render(ctx.dim2);
    gunit_assert("valid reload updates live frames", ctx.draws == 2 && ctx.sources[0].x == 7 && ctx.sources[1].x == 7);
})

GDIM2_TEST_WITH_CACHES(dim2_texture_reload_releases_prior_image, {
    size_t previous = ctx.unloads;
    gunit_assert(
        "write asset",
        gdim2__test_write_asset("glint-dim2-test-atlas.png", "fake png updated with a different size")
    );
    gdim2__test_update(&ctx);
    gunit_assert("texture reload releases prior image", ctx.unloads == previous + 1);
    gdim2_render(ctx.dim2);
    gunit_assert(
        "texture reload preserves atlas frames",
        ctx.draws == 2 && ctx.sources[0].x == 1 && ctx.sources[1].x == 1
    );
})

GDIM2_TEST_WITH_CACHES(dim2_standalone_atlas_values, {
    const char* source =
        "local atlas = Atlas.load('/glint-dim2-test-atlas.json')\n"
        "sprite = atlas:sprite('f')\n"
        "animated = atlas:animation('a')\n"
        "Sprite.set(entity, Sprite.load('/glint-dim2-test-atlas.json', 'f'))\n"
        "assert(Sprite.has(entity))\n"
        "AnimatedSprite.set(entity, AnimatedSprite.load('/glint-dim2-test-atlas.json', 'a'))\n"
        "assert(AnimatedSprite.has(entity))\n"
        "snapshot = Sprite.get(entity)\n"
        "AnimatedSprite.set(entity, AnimatedSprite.get(entity))\n";
    gunit_assert(
        "build sprite components from atlas values",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "atlas_values")
    );
})

GDIM2_TEST_WITH_CACHES(dim2_invalid_atlas_values_preserve_existing_sprites, {
    const char* source =
        "assert(not pcall(function() Sprite.set(entity, '/glint-dim2-test-atlas.json', 'missing') end))\n"
        "assert(not pcall(function() Sprite.set(entity, '/missing.json', 'f') end))\n"
        "assert(not pcall(function() AnimatedSprite.set(entity, '/glint-dim2-test-atlas.json', 'missing') end))\n"
        "assert(not pcall(function() AnimatedSprite.set(entity, '/missing.json', 'a') end))\n"
        "assert(Sprite.has(entity) and AnimatedSprite.has(entity))\n"
        "local atlas = Atlas.load('/glint-dim2-test-atlas.json')\n"
        "assert(not pcall(function() return atlas:sprite('missing') end))\n"
        "assert(not pcall(function() return atlas:animation('missing') end))\n"
        "assert(not pcall(function() return Atlas.load('/missing.json') end))\n"
        "assert(not pcall(function() return Sprite.load('/missing.json', 'f') end))\n"
        "assert(not pcall(function() return AnimatedSprite.load('/glint-dim2-test-atlas.json', 'missing') end))\n";
    gunit_assert(
        "reject missing sprites, animations, and atlas files",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "invalid_atlas_values")
    );
    gdim2_render(ctx.dim2);
    gunit_assert(
        "failed replacement preserves frames",
        ctx.draws == 2 && ctx.sources[0].x == 1 && ctx.sources[1].x == 1
    );
})

GDIM2_TEST_WITH_CACHES(dim2_atlas_values_survive_reload, {
    gunit_assert(
        "save sprite and animation values",
        gdim2__test_run_script(
            ctx.alloc,
            ctx.script,
            "snapshot = Sprite.get(entity)\n"
            "animated = Atlas.load('/glint-dim2-test-atlas.json'):animation('a')\n",
            "save_atlas_values"
        )
    );
    // Different size makes the rewrite visible even with coarse timestamps.
    gunit_assert(
        "write asset",
        gdim2__test_write_asset(
            "glint-dim2-test-atlas.json",
            "{\"frames\": {\"f\":{\"x\":3,\"w\":4,\"h\":5}}, \"animations\":{\"a\":[\"f\"]}}"
        )
    );
    gdim2__test_update(&ctx);
    gunit_assert(
        "assign the saved values to the entity",
        gdim2__test_run_script(
            ctx.alloc,
            ctx.script,
            "Sprite.set(entity, snapshot)\n"
            "AnimatedSprite.set(entity, animated)\n",
            "reuse_atlas_values"
        )
    );
    gdim2_render(ctx.dim2);
    gunit_assert("values resolve reloaded frames", ctx.draws == 2 && ctx.sources[0].x == 3 && ctx.sources[1].x == 3);
})

GUNIT_TEST(dim2_standalone_transform_defaults,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("bind the 2D API", gdim2__test_create_bindings(&ctx, false));

    const char* source =
        "local t = Transform2D.new()\n"
        "assert(t.scale.x == 1 and t.scale.y == 1 and t.position.x == 0 and t.rotation == 0)\n"
        "assert(not pcall(function() return Transform2D.new(Vec2.new()) end))\n";
    gunit_assert(
        "default transform has unit scale and partial construction fails",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "transform_defaults")
    );
)

GUNIT_TEST(dim2_standalone_transform_components,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("bind the 2D API", gdim2__test_create_bindings(&ctx, false));

    const char* source =
        "local t = Transform2D.new()\n"
        "t.position = Vec2.new(3, 4)\n"
        "t.rotation = 90\n"
        "local p = t.position\n"
        "p.x = 5\n"
        "assert(t.position.x == 5 and t.position.y == 4)\n"
        "local full = Transform2D.new(Vec2.new(1, 2), Vec2.new(3, 4), Vec2.new(5, 6), 45)\n"
        "assert(full.scale.x == 3 and full.rotation == 45 and full.origin.y == 6)\n"
        "Transform2D.set(entity, full)\n"
        "local got = Transform2D.get(entity)\n"
        "assert(got.position.x == 1 and got.rotation == 45)\n";
    gunit_assert(
        "set and read transform values",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "transform_components")
    );
)

GUNIT_TEST(dim2_transform_vector_arithmetic,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("bind the 2D API", gdim2__test_create_bindings(&ctx, false));

    const char* source =
        "local t = Transform2D.new()\n"
        "t.position = Vec2.new(8, 12)\n"
        "local p = t.position\n"
        "local v = Vec2.new(2, 3)\n"
        "local function eq(a, x, y) assert(a.x == x and a.y == y) end\n"
        "eq(p + v, 10, 15)\n"
        "eq(v + p, 10, 15)\n"
        "eq(p - v, 6, 9)\n"
        "eq(v - p, -6, -9)\n"
        "eq(p * v, 16, 36)\n"
        "eq(v * p, 16, 36)\n"
        "eq(p / v, 4, 4)\n"
        "eq(v / p, 0.25, 0.25)\n";
    gunit_assert(
        "use position references on either side of each vector operator",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "vector_operators")
    );
)

GUNIT_TEST(dim2_transform_vector_methods_and_references,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("bind the 2D API", gdim2__test_create_bindings(&ctx, false));

    const char* source =
        "local t = Transform2D.new()\n"
        "t.position = Vec2.new(8, 12)\n"
        "local p = t.position\n"
        "local v = Vec2.new(2, 3)\n"
        "local function eq(a, x, y) assert(a.x == x and a.y == y) end\n"
        "eq(p:scale(2), 16, 24)\n"
        "eq(p + t.scale, 9, 13)\n"
        "assert(math.abs(p:length() - math.sqrt(208)) < 1e-5)\n"
        "assert(math.abs(p:normalize():length() - 1) < 1e-5)\n"
        "eq(p, 8, 12)\n"
        "t.position = p + v\n"
        "eq(t.position, 10, 15)\n"
        "eq(p, 10, 15)\n"
        "orphan = Transform2D.new().position\n"
        "collectgarbage()\n"
        "eq(orphan:scale(2), 0, 0)\n";
    gunit_assert(
        "apply vector methods to position references",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "vector_methods")
    );
)

GUNIT_TEST(dim2_removed_component_vector_rejects_saved_methods,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("bind the 2D API", gdim2__test_create_bindings(&ctx, false));

    const char* source =
        "component_vector = Transform2D.get(entity).position\n"
        "local v = Vec2.new(2, 3)\n"
        "assert((component_vector + v).x == 15 and (v + component_vector).y == 20)\n"
        "component_scale = component_vector.scale\n"
        "component_length = component_vector.length\n"
        "component_normalize = component_vector.normalize\n"
        "Transform2D.remove(entity)\n"
        "assert(not pcall(function() return component_vector + v end))\n"
        "assert(not pcall(function() return v + component_vector end))\n"
        "assert(not pcall(function() return component_scale(component_vector, 2) end))\n"
        "assert(not pcall(function() return component_length(component_vector) end))\n"
        "assert(not pcall(function() return component_normalize(component_vector) end))\n";
    gunit_assert(
        "removed transform invalidates saved vector operations",
        gdim2__test_run_script(ctx.alloc, ctx.script, source, "removed_vector")
    );
)

GDIM2_TEST_WITH_CACHES(dim2_rebinding_retains_original_world, {
    gunit_assert("rebind the Lua API with saved references", gdim2__test_rebind(&ctx));
})

GUNIT_TEST(dim2_shared_atlas_lifetime,
    gdim2__TestContext ctx = {0};
    gunit_defer(gdim2__test_destroy, &ctx);
    gunit_assert("load sprites with a shared cache", gdim2__test_create_reload(&ctx, true));

    gunit_assert(
        "shared cache owns the loaded atlas",
        gasset_Cache_contains(ctx.cache, "glint.dim2.atlas", "/glint-dim2-test-atlas.json")
    );

    gunit_assert("destroy original instance", gdim2_destroy(ctx.alloc, ctx.dim2));
    ctx.dim2 = NULL;
    gunit_assert(
        "atlas removed from the shared cache",
        !gasset_Cache_contains(ctx.cache, "glint.dim2.atlas", "/glint-dim2-test-atlas.json")
    );
)

GDIM2_TEST_WITH_CACHES(dim2_destroyed_instance_references_fail_safely, {
    gunit_assert("rebind the Lua API with saved references", gdim2__test_rebind(&ctx));

    gunit_assert("destroy original instance", gdim2_destroy(ctx.alloc, ctx.dim2));
    ctx.dim2 = NULL;
    if (ctx.cache != NULL) {
        gasset_Cache_destroy(ctx.alloc, ctx.cache);
        ctx.cache = NULL;
    }
    const char* destroyed_references =
        "local function check(f, name) assert(not pcall(f), name) end\n"
        "check(function() return saved.rotation end, 'saved')\n"
        "check(function() return vector.x end, 'vector')\n"
        "check(function() return vector + Vec2.new(1, 2) end, 'vector math')\n"
        "check(function() return Vec2.new(1, 2) + vector end, 'reverse vector math')\n"
        "check(function() return component_scale(vector, 2) end, 'saved scale')\n"
        "check(function() return component_length(vector) end, 'saved length')\n"
        "check(function() return component_normalize(vector) end, 'saved normalize')\n"
        "check(function() Sprite.set(entity, snapshot) end, 'set snapshot')\n"
        "check(function() AnimatedSprite.set(entity, animated) end, 'set animated')\n"
        "check(function() Sprite.set(other_entity, snapshot) end, 'cross snapshot')\n"
        "check(function() AnimatedSprite.set(other_entity, animated) end, 'cross animated')\n"
        "assert(other_transform.rotation == 0, 'other')\n"
        "other_transform.position.x = 3\n";
    gunit_assert(
        "reject original references after destruction",
        gdim2__test_run_script(ctx.alloc, ctx.script, destroyed_references, "destroyed_references")
    );
})

gunit_main();

static void gdim2__test_create(gdim2__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    ctx->module = LUA_NOREF;
    gdim2__test_active = ctx;
}

static bool gdim2__test_create_bindings(gdim2__TestContext* ctx, bool shared) {
    gdim2__test_create(ctx);
    ctx->shared = shared;
    ctx->world = gecs_World_new(ctx->alloc);
    ctx->vfs = gvfs_create(ctx->alloc);
    ctx->script = gscript_lua_new(ctx->alloc);
    ctx->owns_script = true;
    if (!gunit_check("binding resources created", ctx->world != NULL && ctx->vfs != NULL && ctx->script != NULL)) {
        return false;
    }
    ctx->cache = shared ? gasset_Cache_create(ctx->alloc) : NULL;
    if (!gunit_check("shared cache created", !shared || ctx->cache != NULL)) {
        return false;
    }
    ctx->dim2 = gdim2_new(ctx->world, ctx->vfs, ctx->cache);
    ctx->owns_dim2 = true;
    if (!gunit_check("2D instance created", ctx->dim2 != NULL)) {
        return false;
    }
    lua_State* lua = gscript_lua_state(ctx->script);
    gecs_ID entity = gecs_spawn(ctx->world).id;
    if (!gunit_check("fixture entity spawned", entity != 0)) {
        return false;
    }
    gscript_bindings_push_entity(lua, (gecs_Entity) {ctx->world, entity});
    lua_setglobal(lua, "entity");
    if (!gunit_check("types bound", gtypes_bindings_bind(ctx->alloc, lua))) {
        return false;
    }
    if (!gunit_check("2D API bound", gdim2_bindings_bind(ctx->alloc, ctx->dim2, lua))) {
        return false;
    }
    const char* source = "Transform2D.set(entity, Vec2.new(13, 17), Vec2.new(2, 3), Vec2.new(), 0)";
    return gunit_check(
        "transform fixture created",
        gdim2__test_run_script(ctx->alloc, ctx->script, source, "transform")
    );
}

static bool gdim2__test_create_reload(gdim2__TestContext* ctx, bool shared) {
    if (!gdim2__test_create_bindings(ctx, shared)) {
        return false;
    }
    ctx->native_fixtures = true;
    if (!gdim2__test_write_asset(
            "glint-dim2-test-atlas.json",
            "{\"frames\":{\"f\":{\"x\":1,\"w\":4,\"h\":5}},\"animations\":{\"a\":[\"f\"]}}"
        )) {
        return false;
    }
    if (!gdim2__test_write_asset("glint-dim2-test-atlas.png", "fake png")) {
        return false;
    }
    if (!gunit_check("native fixtures mounted", gvfs_mount(ctx->alloc, ctx->vfs, "/", "."))) {
        return false;
    }

    const char* source =
        "Sprite.set(entity, '/glint-dim2-test-atlas.json', 'f')\n"
        "AnimatedSprite.set(entity, '/glint-dim2-test-atlas.json', 'a')\n";
    return gdim2__test_run_script(ctx->alloc, ctx->script, source, "live sprites");
}

static bool gdim2__test_create_virtual(gdim2__TestContext* ctx) {
    gdim2__test_create(ctx);
    ctx->world = gecs_World_new(ctx->alloc);
    ctx->vfs = gvfs_create(ctx->alloc);
    ctx->script = gscript_lua_new(ctx->alloc);
    ctx->owns_script = true;
    if (!gunit_check("virtual resources created", ctx->world != NULL && ctx->vfs != NULL && ctx->script != NULL)) {
        return false;
    }
    gvfs_Filesystem backend =
        {.exists = gdim2__test_virtual_exists, .read = gdim2__test_virtual_read, .list = gdim2__test_virtual_list};
    if (!gunit_check("virtual filesystem mounted", gvfs_mount_filesystem(ctx->alloc, ctx->vfs, "/", backend))) {
        return false;
    }
    ctx->dim2 = gdim2_new(ctx->world, ctx->vfs, NULL);
    ctx->owns_dim2 = true;
    if (!gunit_check("2D instance created", ctx->dim2 != NULL)) {
        return false;
    }
    lua_State* lua = gscript_lua_state(ctx->script);
    if (!gunit_check("types bound", gtypes_bindings_bind(ctx->alloc, lua))) {
        return false;
    }
    return gunit_check("2D API bound", gdim2_bindings_bind(ctx->alloc, ctx->dim2, lua));
}

static bool gdim2__test_create_texture(gdim2__TestContext* ctx) {
    gdim2__test_create(ctx);
    ctx->texture_fixture = true;
    if (!gdim2__test_write_asset("glint-shared-texture.png", "first image")) {
        return false;
    }
    ctx->vfs = gvfs_create(ctx->alloc);
    ctx->cache = gasset_Cache_create(ctx->alloc);
    if (!gunit_check("texture resources created", ctx->vfs != NULL && ctx->cache != NULL)) {
        return false;
    }
    if (!gunit_check("texture fixture mounted", gvfs_mount(ctx->alloc, ctx->vfs, "/", "."))) {
        return false;
    }
    ctx->textures = gtexture_new(ctx->alloc, ctx->cache, ctx->vfs);
    return gunit_check("texture registration created", ctx->textures != NULL);
}

static bool gdim2__test_create_module(gdim2__TestContext* ctx) {
    gdim2__test_create(ctx);
    if (!gunit_check("asset root argument supplied", gunit_argc == 2)) {
        return false;
    }
    ctx->world = gecs_World_new(ctx->alloc);
    if (!gunit_check("module world created", ctx->world != NULL)) {
        return false;
    }
    ctx->modules = gmod_Modules_new(ctx->world);
    if (!gunit_check("module registry created", ctx->modules != NULL)) {
        return false;
    }
    ctx->config.asset_root = gunit_argv[1];
    ctx->clock.delta = 0.2f;
    if (!gunit_check(
            "host config installed",
            gecs_context_set(ctx->world, GECS_CONTEXT(ctx->world, glint_Config), &ctx->config) != NULL
        )) {
        return false;
    }
    if (!gunit_check(
            "host clock installed",
            gecs_context_set(ctx->world, GECS_CONTEXT(ctx->world, gtime_FrameTime), &ctx->clock) != NULL
        )) {
        return false;
    }
    if (!gunit_check(
            "host window installed",
            gecs_context_set(ctx->world, GECS_CONTEXT(ctx->world, gwindow_Host), &ctx->window) != NULL
        )) {
        return false;
    }
    gmod_Module descriptor = gdim2_module();
    return gunit_check("dim2 and dependencies imported", gmod_Modules_import(ctx->alloc, ctx->modules, descriptor));
}

static bool gdim2__test_load_example(gdim2__TestContext* ctx) {
    if (!gunit_check("modules loaded", gmod_Modules_load(ctx->alloc, ctx->modules))) {
        return false;
    }
    gscript_Service* service = gecs_context_get(ctx->world, GECS_CONTEXT(ctx->world, gscript_Service));
    if (!gunit_check("script service published", service != NULL && service->script != NULL)) {
        return false;
    }
    ctx->script = service->script;
    lua_State* lua = gscript_lua_state(ctx->script);
    lua_pushlightuserdata(lua, ctx->world);
    lua_pushcclosure(lua, gdim2__test_spawn, 1);
    lua_setglobal(lua, "spawn");
    ctx->vfs = gvfs_create(ctx->alloc);
    if (!gunit_check(
            "example filesystem created",
            ctx->vfs != NULL && gvfs_mount(ctx->alloc, ctx->vfs, "/", gunit_argv[1])
        )) {
        return false;
    }
    size_t size = 0;
    ctx->source = gvfs_read(ctx->alloc, ctx->vfs, "/game.lua", &size);
    if (!gunit_check("example source read", ctx->source != NULL)) {
        return false;
    }
    ctx->module = gscript_lua_load_module(ctx->alloc, ctx->script, ctx->source, size, "game.lua");
    galloc_free(ctx->alloc, ctx->source);
    ctx->source = NULL;
    if (!gunit_check("example module loaded", ctx->module != LUA_NOREF)) {
        return false;
    }
    gecs_ID entity = gecs_spawn(ctx->world).id;
    if (!gunit_check("example entity spawned", entity != 0)) {
        return false;
    }
    return gunit_check(
        "example setup called",
        gscript_lua_call_module(ctx->script, ctx->module, "setup", (gecs_Entity) {ctx->world, entity})
    );
}

static bool gdim2__test_create_other_world(gdim2__TestContext* ctx) {
    ctx->other_world = gecs_World_new(ctx->alloc);
    if (!gunit_check("second world created", ctx->other_world != NULL)) {
        return false;
    }
    ctx->other_dim2 = gdim2_new(ctx->other_world, ctx->vfs, NULL);
    if (!gunit_check("second 2D instance created", ctx->other_dim2 != NULL)) {
        return false;
    }
    gecs_ID other = gecs_spawn(ctx->other_world).id;
    if (!gunit_check("second entity spawned", other != 0)) {
        return false;
    }
    lua_State* lua = gscript_lua_state(ctx->script);
    gscript_bindings_push_entity(lua, (gecs_Entity) {ctx->other_world, other});
    lua_setglobal(lua, "other_entity");
    return gunit_check("second 2D instance bound", gdim2_bindings_bind(ctx->alloc, ctx->other_dim2, lua));
}

static bool gdim2__test_rebind(gdim2__TestContext* ctx) {
    const char* save_references =
        "saved = Transform2D.get(entity)\n"
        "vector = saved.position\n"
        "snapshot = Sprite.get(entity)\n"
        "animated = Atlas.load('/glint-dim2-test-atlas.json'):animation('a')\n"
        "component_vector = Transform2D.get(entity).position\n"
        "local v = Vec2.new(2, 3)\n"
        "assert((component_vector + v).x == 15 and (v + component_vector).y == 20)\n"
        "component_scale = component_vector.scale\n"
        "component_length = component_vector.length\n"
        "component_normalize = component_vector.normalize\n";
    if (!gdim2__test_run_script(ctx->alloc, ctx->script, save_references, "save references")) {
        return false;
    }
    if (!gunit_check("create other world", gdim2__test_create_other_world(ctx))) {
        return false;
    }
    const char* rebind_references =
        "assert(saved.position.x == 13)\n"
        "assert((saved.position + Vec2.new(1, 2)).y == 19)\n"
        "assert(saved.position:scale(2).x == 26)\n"
        "Transform2D.set(other_entity)\n"
        "assert(not pcall(function() Transform2D.set(other_entity, saved) end))\n"
        "assert(not pcall(function() Sprite.set(other_entity, snapshot) end))\n"
        "assert(not pcall(function() AnimatedSprite.set(other_entity, animated) end))\n"
        "other_transform = Transform2D.get(other_entity)\n";
    return gdim2__test_run_script(ctx->alloc, ctx->script, rebind_references, "rebind references");
}

static void gdim2__test_update(gdim2__TestContext* ctx) {
    if (ctx->shared) {
        gasset_Cache_poll(ctx->alloc, ctx->cache);
    }
    gdim2_update(ctx->alloc, ctx->dim2, 0);
}

static void gdim2__test_destroy(void* user) {
    gdim2__TestContext* ctx = user;
    if (ctx->source != NULL) {
        galloc_free(ctx->alloc, ctx->source);
    }
    if (ctx->module != LUA_NOREF && ctx->script != NULL) {
        gscript_lua_release_module(ctx->alloc, ctx->script, ctx->module);
    }
    if (ctx->modules != NULL) {
        gmod_Modules_free(ctx->alloc, ctx->modules);
    }
    if (ctx->owns_dim2 && ctx->dim2 != NULL) {
        gdim2_destroy(ctx->alloc, ctx->dim2);
    }
    if (ctx->owns_script && ctx->script != NULL) {
        gscript_lua_destroy(ctx->alloc, ctx->script);
    }
    if (ctx->other_dim2 != NULL) {
        gdim2_destroy(ctx->alloc, ctx->other_dim2);
    }
    if (ctx->other_world != NULL) {
        gecs_World_free(ctx->other_world);
    }
    if (ctx->textures != NULL) {
        gtexture_destroy(ctx->alloc, ctx->textures);
    }
    if (ctx->cache != NULL) {
        gasset_Cache_destroy(ctx->alloc, ctx->cache);
    }
    if (ctx->vfs != NULL) {
        gvfs_destroy(ctx->alloc, ctx->vfs);
    }
    if (ctx->world != NULL) {
        gecs_World_free(ctx->world);
    }
    if (ctx->native_fixtures) {
        remove("glint-dim2-test-atlas.json");
        remove("glint-dim2-test-atlas.png");
    }
    if (ctx->texture_fixture) {
        remove("glint-shared-texture.png");
    }
    gdim2__test_active = NULL;
}

static int gdim2__test_spawn(lua_State* lua) {
    gecs_World* world = lua_touserdata(lua, lua_upvalueindex(1));
    gecs_ID entity = gecs_spawn(world).id;
    if (entity == 0) {
        return luaL_error(lua, "failed to spawn");
    }

    gscript_bindings_push_entity(lua, (gecs_Entity) {world, entity});
    return 1;
}

static bool gdim2__test_write_asset(const char* path, const char* data) {
    FILE* file = fopen(path, "wb");
    if (!gunit_check("open fixture", file != NULL)) {
        return false;
    }
    size_t written = fwrite(data, 1, strlen(data), file);
    int closed = fclose(file);
    if (!gunit_check("write fixture contents", written == strlen(data))) {
        return false;
    }
    return gunit_check("close fixture", closed == 0);
}

static bool gdim2__test_run_script(
    galloc_Allocator alloc,
    gscript_lua_Runtime* script,
    const char* source,
    const char* name
) {
    if (gscript_lua_run(alloc, script, source, strlen(source), name)) {
        return true;
    }

    lua_State* lua = gscript_lua_state(script);
    const char* message = lua_tostring(lua, -1);
    bool succeeded = gunit_check(message != NULL ? message : "Lua chunk failed", false);
    lua_pop(lua, 1);
    return succeeded;
}

static bool gdim2__test_virtual_exists(galloc_Allocator alloc, void* context, const char* path) {
    (void)alloc;
    (void)context;
    return strcmp(path, "atlas.json") == 0 || strcmp(path, "atlas.png") == 0;
}

static void* gdim2__test_virtual_read(galloc_Allocator alloc, void* context, const char* path, size_t* size) {
    (void)context;
    const char* content =
        strcmp(path, "atlas.json") == 0 ? "{\"frames\":{\"f\":{\"x\":1,\"w\":4,\"h\":5}},\"animations\":{}}" : "png";
    size_t length = strlen(content);
    char* copy = galloc_alloc(alloc, length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, content, length);
    if (size != NULL) {
        *size = length;
    }

    return copy;
}

static bool gdim2__test_virtual_list(
    galloc_Allocator alloc,
    void* context,
    const char* path,
    gvfs_EntryFn visit,
    void* user
) {
    (void)alloc;
    (void)context;
    (void)path;
    (void)visit;
    (void)user;
    return false;
}

static bool gdim2__test_unload_during_load(galloc_Allocator alloc, void* instance, const char* path, void* user) {
    (void)instance;
    (void)path;
    gdim2__TestUnload* callback = user;
    if (!gunit_check("2D instance is busy during cache callback", gdim2_busy(callback->dim2))) {
        return false;
    }
    if (!gunit_check("busy destruction leaves instance intact", !gdim2_destroy(alloc, callback->dim2))) {
        return false;
    }
    return gunit_check("busy module vetoes unload", !gmod_Modules_unload(alloc, callback->modules));
}

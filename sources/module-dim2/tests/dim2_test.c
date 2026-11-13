#include <glint/dim2/dim2.h>
#include <glint/script.h>
#include <glint/script_entity.h>
#include <glint/types_lua.h>

#include <mewunit.h>
#include <lauxlib.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static unsigned gdim2__test_draws;
static unsigned gdim2__test_unloads;
static Rectangle gdim2__test_sources[16];
static Rectangle gdim2__test_destinations[16];

/* Stand-in graphics functions allow exercising asset loading and rendering in
 * CI without opening a window; the PNG bytes still come from the actual VFS. */
Image LoadImageFromMemory(const char *type, const unsigned char *bytes, int size) {
    mewrequire("Test expectation failed", strcmp(type, ".png") == 0 && bytes && size > 0);
    return (Image) {.data = (void *)bytes, .width = 64, .height = 16, .mipmaps = 1, .format = 7};
}

bool IsImageValid(Image image) {
    return image.data != NULL;
}

void UnloadImage(Image image) {
    (void)image;
}

Texture2D LoadTextureFromImage(Image image) {
    mewrequire("Test expectation failed", image.data);
    return (Texture2D) {.id = 1, .width = 64, .height = 16, .mipmaps = 1, .format = 7};
}

bool IsTextureValid(Texture2D texture) {
    return texture.id != 0;
}

void UnloadTexture(Texture2D texture) {
    mewrequire("Test expectation failed", texture.id);
    gdim2__test_unloads++;
}

void DrawTexturePro(Texture2D texture, Rectangle src, Rectangle dest, Vector2 origin, float rotation, Color tint) {
    mewrequire(
        "Test expectation failed",
        texture.id && origin.x == 0 && origin.y == 0 && rotation == 0 && tint.a == 255
    );
    mewrequire("Test expectation failed", gdim2__test_draws < 16);
    gdim2__test_sources[gdim2__test_draws] = src;
    gdim2__test_destinations[gdim2__test_draws++] = dest;
}

static int gdim2__test_spawn(lua_State *lua) {
    gecs_World *world = lua_touserdata(lua, lua_upvalueindex(1));
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    if (!entity)
        return luaL_error(lua, "failed to spawn");
    gscript_push_entity(alloc, lua, world, entity);
    return 1;
}

static int gdim2__test_log_info(lua_State *lua) {
    (void)lua;
    return 0;
}

static void gdim2__test_write_asset(const char *path, const char *data) {
    FILE *file = fopen(path, "wb");
    mewassert("Test expectation failed", file);
    mewassert("Test expectation failed", fwrite(data, 1, strlen(data), file) == strlen(data));
    mewassert("Test expectation failed", fclose(file) == 0);
}

static bool gdim2__test_run_script(galloc_Allocator alloc, gscript_Script *script, const char *source, const char *name) {
    if (gscript_run(alloc, script, source, strlen(source), name)) {
        return true;
    }
    lua_State *lua = gscript_state(script);
    const char *message = lua_tostring(lua, -1);
    fprintf(stderr, "lua %s: %s\n", name, message ? message : "unknown error");
    lua_pop(lua, 1);
    return false;
}

static void gdim2__test_test_reload(galloc_Allocator alloc) {
    const char *json = "glint-dim2-test-atlas.json";
    const char *png = "glint-dim2-test-atlas.png";
    gdim2__test_write_asset(
        json,
        "{\"frames\":{\"f\":{\"x\":1,\"w\":4,\"h\":5}},"
        "\"animations\":{\"a\":[\"f\"]}}"
    );
    gdim2__test_write_asset(png, "fake png");
    gecs_World *world = gecs_World_new(alloc);
    gvfs_Vfs *vfs = gvfs_create(alloc);
    gscript_Script *script = gscript_new(alloc);
    mewassert("Test expectation failed", world && vfs && script && gvfs_mount(alloc, vfs, "/", "."));
    gdim2_Dim2 *dim2 = gdim2_new(alloc, world, vfs);
    mewassert("Test expectation failed", dim2);
    lua_State *lua = gscript_state(script);
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    gscript_push_entity(alloc, lua, world, entity);
    lua_setglobal(lua, "entity");
    mewassert("types registered", gtypes_bind_lua(alloc, lua));
    mewassert("Test expectation failed", gdim2_bind_lua(alloc, dim2, lua));
    const char *code =
        "Transform2D.set(entity); Sprite.set(entity, '/glint-dim2-test-atlas.json', 'f'); "
        "AnimatedSprite.set(entity, '/glint-dim2-test-atlas.json', 'a')";
    mewassert("Test expectation failed", gscript_run(alloc, script, code, strlen(code), "reload-test"));
    const char *mutation =
        "saved = Transform2D.get(entity); saved.position.x = 13; saved.position.y = 17; "
        "saved.scale = Vec2.new(2, 3); saved.origin = Vec2.new(); saved.rotation = 0; "
        "assert(Transform2D.get(entity).position.x == 13 and saved.scale.y == 3); "
        "Transform2D.set(entity, saved); vector = saved.position; "
        "Transform2D.remove(entity); assert(not pcall(function() return saved.rotation end)); "
        "assert(not pcall(function() return vector.x end)); "
        "Transform2D.set(entity, Vec2.new(13,17), Vec2.new(2,3), Vec2.new(), 0)";
    mewassert("transform mutations and removed handles", gscript_run(alloc, script, mutation, strlen(mutation), "transform"));
    gdim2__test_draws = 0;
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 2 && gdim2__test_sources[0].x == 1 && gdim2__test_sources[1].x == 1);
    mewassert("transform edits affect rendered destination", gdim2__test_destinations[0].x == 13 && gdim2__test_destinations[0].y == 17 &&
              gdim2__test_destinations[0].width == 8 && gdim2__test_destinations[0].height == 15);
    gdim2__test_write_asset(json, "invalid JSON");
    gdim2_update(alloc, dim2, 0);
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 4 && gdim2__test_sources[2].x == 1 && gdim2__test_sources[3].x == 1);
    gdim2__test_write_asset(json, "{\"frames\":{\"other\":{}},\"animations\":{}}");
    gdim2_update(alloc, dim2, 0);
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 6 && gdim2__test_sources[4].x == 1 && gdim2__test_sources[5].x == 1);
    gdim2__test_write_asset(
        json,
        "{\"frames\":{\"f\":{\"x\":7,\"w\":9,\"h\":5}},"
        "\"animations\":{\"a\":[\"f\"]}}"
    );
    gdim2_update(alloc, dim2, 0);
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 8 && gdim2__test_sources[6].x == 7 && gdim2__test_sources[7].x == 7);
    unsigned old_unloads = gdim2__test_unloads;
    gdim2__test_write_asset(png, "fake png updated with a different size");
    gdim2_update(alloc, dim2, 0);
    mewassert("Test expectation failed", gdim2__test_unloads == old_unloads + 1);
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 10 && gdim2__test_sources[8].x == 7 && gdim2__test_sources[9].x == 7);
    const char *values =
        "local atlas = Atlas.load('/glint-dim2-test-atlas.json'); "
        "sprite = atlas:sprite('f'); animated = atlas:animation('a'); "
        "assert(not pcall(function() return atlas:sprite('missing') end)); "
        "assert(not pcall(function() return atlas:animation('missing') end)); "
        "assert(not pcall(function() return Atlas.load('/missing.json') end)); "
        "assert(not pcall(function() return Sprite.load('/missing.json', 'f') end)); "
        "assert(not pcall(function() return AnimatedSprite.load('/glint-dim2-test-atlas.json', 'missing') end)); "
        "Sprite.set(entity, Sprite.load('/glint-dim2-test-atlas.json', 'f')); "
        "assert(Sprite.has(entity)); "
        "AnimatedSprite.set(entity, AnimatedSprite.load('/glint-dim2-test-atlas.json', 'a')); "
        "assert(AnimatedSprite.has(entity)); "
        "snapshot = Sprite.get(entity); "
        "AnimatedSprite.set(entity, AnimatedSprite.get(entity))";
    mewassert("standalone atlas values", gdim2__test_run_script(alloc, script, values, "values"));
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 12 && gdim2__test_sources[10].x == 7 && gdim2__test_sources[11].x == 7);
    /* Values survive an atlas reload that keeps their names. */
    gdim2__test_write_asset(
        json,
        "{\"frames\":{\"f\":{\"x\":3,\"w\":4,\"h\":5}},"
        "\"animations\":{\"a\":[\"f\"]}}"
    );
    gdim2_update(alloc, dim2, 0);
    const char *reuse = "Sprite.set(entity, snapshot); AnimatedSprite.set(entity, animated)";
    mewassert("values survive reloads", gdim2__test_run_script(alloc, script, reuse, "reuse"));
    gdim2_render(dim2);
    mewassert("Test expectation failed", gdim2__test_draws == 14 && gdim2__test_sources[12].x == 3 && gdim2__test_sources[13].x == 3);
    const char *value_transform =
        "local t = Transform2D.new(); "
        "assert(t.scale.x == 1 and t.scale.y == 1 and t.position.x == 0 and t.rotation == 0); "
        "t.position = Vec2.new(3, 4); t.rotation = 90; "
        "local p = t.position; p.x = 5; "
        "assert(t.position.x == 5 and t.position.y == 4); "
        "assert(not pcall(function() return Transform2D.new(Vec2.new()) end)); "
        "local full = Transform2D.new(Vec2.new(1, 2), Vec2.new(3, 4), Vec2.new(5, 6), 45); "
        "assert(full.scale.x == 3 and full.rotation == 45 and full.origin.y == 6); "
        "Transform2D.set(entity, full); "
        "local got = Transform2D.get(entity); "
        "assert(got.position.x == 1 and got.rotation == 45); "
        "Transform2D.set(entity, Vec2.new(13, 17), Vec2.new(2, 3), Vec2.new(), 0)";
    mewassert("standalone transform values", gdim2__test_run_script(alloc, script, value_transform, "tvalues"));
    const char *vector_math =
        "local t = Transform2D.new(); t.position = Vec2.new(8, 12); "
        "local p = t.position; local v = Vec2.new(2, 3); "
        "local function eq(a, x, y) assert(a.x == x and a.y == y) end; "
        "eq(p + v, 10, 15); eq(v + p, 10, 15); eq(p - v, 6, 9); eq(v - p, -6, -9); "
        "eq(p * v, 16, 36); eq(v * p, 16, 36); eq(p / v, 4, 4); eq(v / p, .25, .25); "
        "eq(p:scale(2), 16, 24); eq(p + t.scale, 9, 13); "
        "t.position = p + v; eq(t.position, 10, 15); eq(p, 10, 15); "
        "orphan = Transform2D.new().position; collectgarbage(); eq(orphan:scale(2), 0, 0); "
        "component_vector = Transform2D.get(entity).position; "
        "eq(component_vector + v, 15, 20); eq(v + component_vector, 15, 20); "
        "component_scale = component_vector.scale; Transform2D.remove(entity); "
        "assert(not pcall(function() return component_vector + v end)); "
        "assert(not pcall(function() return v + component_vector end)); "
        "assert(not pcall(function() return component_scale(component_vector, 2) end)); "
        "Transform2D.set(entity, Vec2.new(13, 17), Vec2.new(2, 3), Vec2.new(), 0)";
    mewassert("transform vector arithmetic", gdim2__test_run_script(alloc, script, vector_math, "vector-math"));
    gecs_World *other_world = gecs_World_new(alloc);
    gdim2_Dim2 *other_dim2 = gdim2_new(alloc, other_world, vfs);
    gecs_Entity other = gecs_Entity_new(alloc, other_world);
    mewassert("second world and module created", other_world && other_dim2 && other);
    gscript_push_entity(alloc, lua, other_world, other);
    lua_setglobal(lua, "other_entity");
    mewassert("second module bound", gdim2_bind_lua(alloc, other_dim2, lua));
    const char *rebind = "assert(saved.position.x == 13); assert((saved.position + Vec2.new(1,2)).y == 19); "
                         "assert(saved.position:scale(2).x == 26); Transform2D.set(other_entity); "
                         "assert(not pcall(function() Transform2D.set(other_entity, saved) end)); "
                         "assert(not pcall(function() Sprite.set(other_entity, snapshot) end)); "
                         "assert(not pcall(function() AnimatedSprite.set(other_entity, animated) end)); "
                         "other_transform = Transform2D.get(other_entity)";
    mewassert("references retain original world after rebind", gscript_run(alloc, script, rebind, strlen(rebind), "rebind"));
    gdim2_destroy(alloc, dim2);
    const char *destroyed = "local function check(f, name) assert(not pcall(f), name) end; "
                            "check(function() return saved.rotation end, 'saved'); "
                             "check(function() return vector.x end, 'vector'); "
                             "check(function() return vector + Vec2.new(1, 2) end, 'vector math'); "
                             "check(function() return Vec2.new(1, 2) + vector end, 'reverse vector math'); "
                             "check(function() return component_scale(vector, 2) end, 'saved scale'); "
                            "check(function() Sprite.set(entity, snapshot) end, 'set snapshot'); "
                            "check(function() AnimatedSprite.set(entity, animated) end, 'set animated'); "
                            /* Values from the destroyed module are rejected by the live one. */
                            "check(function() Sprite.set(other_entity, snapshot) end, 'cross snapshot'); "
                            "check(function() AnimatedSprite.set(other_entity, animated) end, 'cross animated'); "
                            "assert(other_transform.rotation == 0, 'other'); other_transform.position.x = 3";
    mewassert("destroyed module references fail safely", gdim2__test_run_script(alloc, script, destroyed, "destroyed"));
    gscript_destroy(alloc, script);
    gdim2_destroy(alloc, other_dim2);
    gecs_World_free(alloc, other_world);
    gvfs_destroy(alloc, vfs);
    gecs_World_free(alloc, world);
    mewassert("Test expectation failed", remove(json) == 0);
    mewassert("Test expectation failed", remove(png) == 0);
}

TEST(dim2_test, {
    mewassert("Test expectation failed", mew_test_argc == 2);
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gvfs_Vfs *vfs = gvfs_create(alloc);
    mewassert("Test expectation failed", world && vfs && gvfs_mount(alloc, vfs, "/", mew_test_argv[1]));
    gscript_Script *script = gscript_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    mewassert("module setup", script && modules);
    lua_State *lua = gscript_state(script);
    float delta = 0.2f;
    gtypes_LuaService lua_service = {.lua = lua};
    gdim2_Host host = {.vfs = vfs, .script = script, .delta_time = &delta, .enabled = true};
    gecs_Component lua_type = GECS_CONTEXT(alloc, world, gtypes_LuaService);
    gecs_Component host_type = GECS_CONTEXT(alloc, world, gdim2_Host);
    mewrequire("host services", gecs_context_set(alloc, world, lua_type, &lua_service) &&
               gecs_context_set(alloc, world, host_type, &host));
    gmod_Module descriptor = gdim2_module();
    mewrequire("import dim2 and its types dependency", gmod_Modules_import(alloc, modules, &descriptor));
    gmod_Module types = gtypes_module();
    mewrequire("duplicate imports", gmod_Modules_import(alloc, modules, &descriptor) &&
               gmod_Modules_import(alloc, modules, &types));
    mewrequire("load modules", gmod_Modules_load(alloc, modules));
    gecs_Component service_type = GECS_CONTEXT(alloc, world, gdim2_Service);
    gdim2_Service *service = gecs_context_get(world, service_type);
    mewrequire("2D instance published", service && service->instance);
    lua_pushlightuserdata(lua, world);
    lua_pushcclosure(lua, gdim2__test_spawn, 1);
    lua_setglobal(lua, "spawn");
    lua_pushcfunction(lua, gdim2__test_log_info);
    lua_setglobal(lua, "log_info");
    size_t size = 0;
    char *source = gvfs_read(alloc, vfs, "/game.lua", &size);
    mewassert("Test expectation failed", source);
    int module = gscript_load_module(alloc, script, source, size, "game.lua");
    galloc_free(alloc, source);
    mewassert("Test expectation failed", module != LUA_NOREF);
    gecs_Entity entity = gecs_Entity_new(alloc, world);
    mewassert("Test expectation failed", gscript_call_module(alloc, script, module, "setup", world, entity));
    gmod_Modules_render(alloc, modules);
    mewassert("Test expectation failed", gdim2__test_draws == 2 && gdim2__test_destinations[0].x == 96 && gdim2__test_destinations[1].x == 224);
    mewassert("Test expectation failed", gdim2__test_sources[0].x == 0 && gdim2__test_sources[1].x == 0);
    gmod_Modules_update(alloc, modules);
    gmod_Modules_render(alloc, modules);
    mewassert("Test expectation failed", gdim2__test_draws == 4 && gdim2__test_sources[3].x == 16);
    gscript_release_module(alloc, script, module);
    gmod_Modules_unload(alloc, modules);
    mewassert("service removed on unload", !gecs_context_get(world, service_type));
    gmod_Modules_free(alloc, modules);
    gscript_destroy(alloc, script);
    mewassert("Test expectation failed", gdim2__test_unloads == 1);
    gvfs_destroy(alloc, vfs);
    gecs_World_free(alloc, world);
    gdim2__test_test_reload(alloc);
})

TEST(dim2_missing_services, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    gmod_Module dim2 = gdim2_module();
    mewrequire("import descriptor", gmod_Modules_import(alloc, modules, &dim2));
    mewassert("missing Lua service fails dependency load", !gmod_Modules_load(alloc, modules));
    gscript_Script *script = gscript_new(alloc);
    gtypes_LuaService lua_service = {.lua = gscript_state(script)};
    gecs_Component type = GECS_CONTEXT(alloc, world, gtypes_LuaService);
    mewrequire("install Lua service", gecs_context_set(alloc, world, type, &lua_service));
    mewassert("missing 2D host fails cleanly", !gmod_Modules_load(alloc, modules));
    type = GECS_CONTEXT(alloc, world, gdim2_Service);
    mewassert("no partial instance", !gecs_context_get(world, type));
    gmod_Modules_free(alloc, modules);
    gscript_destroy(alloc, script);
    gecs_World_free(alloc, world);
})

mewtest_main();

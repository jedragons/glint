#include <glint/game.h>
#include <glint/audio.h>
#include <glint/dim2/dim2.h>
#include <glint/types_lua.h>
#include <glint/time_lua.h>
#include <glint/script_entity.h>
#include <glint/time.h>

#include <mewunit.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define GGAME_TEST_CHDIR _chdir
#else
#include <unistd.h>
#define GGAME_TEST_CHDIR chdir
#endif

typedef struct ggame__TestCounts {
    int imported;
    int load;
    int update;
    int render;
    int unload;
    void *scratch;
    bool context_destroyed;
} ggame__TestCounts;

typedef struct ggame__TestLifetimeProbe {
    galloc_Allocator alloc;
    gvfs_Vfs *vfs;
    gscript_Script *script;
    ggame__TestCounts *counts;
} ggame__TestLifetimeProbe;

static bool ggame__test_on_import(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    const gmod_ModuleConfig *config = gmod_Modules_config(modules, "test");
    const gmod_ModuleConfigEntry *entry = gmod_ModuleConfig_get(config, "message");
    if (!entry || entry->kind != GMOD_STRING || strcmp(entry->value.string, "from game") != 0) return false;
    ((ggame__TestCounts *)user)->imported++;
    return true;
}

static void ggame__test_destroy_probe(void *value) {
    ggame__TestLifetimeProbe *probe = value;
    size_t length = 0;
    void *source = gvfs_read(probe->alloc, probe->vfs, "/game.lua", &length);
    mewrequire("VFS must outlive world contexts", source && length);
    lua_State *lua = gscript_state(probe->script);
    lua_getglobal(lua, "Timer");
    mewrequire("Lua state must outlive world contexts", lua_istable(lua, -1));
    lua_pop(lua, 1);
    galloc_free(probe->alloc, source);
    probe->counts->context_destroyed = true;
}

static bool ggame__test_on_load(galloc_Allocator alloc, gecs_World *world, void *user) {
    gecs_Component services_type = GECS_CONTEXT(alloc, world, ggame_Services);
    ggame_Services *services = gecs_context_get(world, services_type);
    mewrequire("Game expectation failed", services && services->vfs && services->script);
    gecs_Component type = gecs_context_register(alloc, world, "LifetimeProbe", sizeof(ggame__TestLifetimeProbe), ggame__test_destroy_probe);
    ggame__TestLifetimeProbe probe = {.alloc = alloc, .vfs = services->vfs, .script = services->script, .counts = user};
    mewrequire("Could not register lifetime probe", type && gecs_context_set(alloc, world, type, &probe));
    ((ggame__TestCounts *)user)->load++;
    return true;
}

static void ggame__test_on_unload(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    ((ggame__TestCounts *)user)->unload++;
}

static void ggame__test_on_update(galloc_Allocator alloc, gecs_World *world, void *user) {
    ggame__TestCounts *counts = user;
    gecs_Component type = GECS_CONTEXT(alloc, world, gmod_FrameAllocator);
    gmod_FrameAllocator *frame = gecs_context_get(world, type);
    mewrequire("Game expectation failed", frame);
    void *scratch = galloc_alloc(frame->alloc, 32);
    mewrequire("Game expectation failed", scratch);
    if (counts->scratch) {
        mewrequire("Game expectation failed", scratch == counts->scratch);
    }
    counts->scratch = scratch;
    memset(scratch, 42, 32);
    counts->update++;
}

static void ggame__test_on_render(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    ggame__TestCounts *counts = user;
    mewrequire("Game expectation failed", ((unsigned char *)counts->scratch)[0] == 42);
    counts->render++;
}

static int ggame__test_finalizer_result(lua_State *lua) {
    int *result = lua_touserdata(lua, lua_upvalueindex(1));
    *result = lua_toboolean(lua, 1) ? 1 : -1;
    return 0;
}

static const char *ggame__test_asset_root;

TEST(headless_audio_import, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gmod_Module modules[] = {gaudio_module(), gaudio_module(), gdim2_module(),
                             gdim2_module(), gtypes_module(), gtime_module()};
    ggame_Config config = {.asset_root = ggame__test_asset_root, .headless = true,
                              .modules = modules, .module_count = sizeof(modules) / sizeof(modules[0])};
    ggame_Game *game = ggame_new(alloc, &config);
    mewrequire("headless game imports audio", game);
    gecs_World *world = ggame_world(game);
    gecs_Component type = GECS_CONTEXT(alloc, world, gaudio_Service);
    gaudio_Service *service = gecs_context_get(world, type);
    mewassert("headless host remains unavailable", service && !service->device.open && !service->module_owned);
    gecs_Component dim2_type = GECS_CONTEXT(alloc, world, gdim2_Service);
    gdim2_Service *dim2 = gecs_context_get(world, dim2_type);
    mewassert("headless 2D module is loaded without graphics", dim2 && !dim2->instance);
    mewassert("headless game steps", ggame_frame(alloc, game, 0.1f));
    ggame_destroy(alloc, game);
})

TEST(game_lifecycle, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame__TestCounts counts = {0};
    gmod_ModuleConfigEntry value = {.key = "message", .kind = GMOD_STRING, .value.string = "from game"};
    gmod_Module module = {
        GMOD_INIT,
        .name = "test",
        .user = &counts,
        .import = ggame__test_on_import,
        .load = ggame__test_on_load,
        .unload = ggame__test_on_unload,
        .update = ggame__test_on_update,
        .render = ggame__test_on_render,
        .config = {.entries = &value, .count = 1}
    };
    ggame_Config config = {
        .asset_root = ggame__test_asset_root,
        .initial_script = "/game.lua",
        .modules = &module,
        .module_count = 1,
        .headless = true
    };
    ggame_Game *game = ggame_new(alloc, &config);
    mewassert("Game expectation failed", game && counts.imported == 1 && counts.load == 1);
    gecs_World *world = ggame_world(game);
    gecs_Component timer_type = gecs_Component_find(world, "gtime_Timer");
    mewassert("Game expectation failed", timer_type && gecs_Component_count(world, timer_type) == 1);
    gecs_Entity entity = gecs_Component_entity(world, timer_type, 0);
    gtime_Timer *timer = gecs_Component_get(world, entity, timer_type);
    mewassert("Game expectation failed", timer && timer->duration == 2.0f && timer->mode == GTIME_TIMER_REPEATING);
    mewassert("Game expectation failed", ggame_frame(alloc, game, 2.1f));
    timer = gecs_Component_get(world, entity, timer_type);
    mewassert("Game expectation failed", timer->is_finished && timer->times_finished_this_tick == 1);
    mewassert("Game expectation failed", counts.update == 1 && counts.render == 1);
    mewassert("Game expectation failed", ggame_frame(alloc, game, 0.1f));
    mewassert("Game expectation failed", counts.update == 2 && counts.render == 2);
    mewassert("Game expectation failed", !strcmp(ggame_error(game), ""));
    ggame_destroy(alloc, game);
    mewassert("Game expectation failed", counts.unload == 1);
    mewassert("Game expectation failed", counts.context_destroyed);
})

TEST(default_mount_snapshots_working_directory, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    char *original = gvfs_get_working_directory(alloc);
    mewassert("get original working directory", original);
    mewassert("enter asset directory", GGAME_TEST_CHDIR(ggame__test_asset_root) == 0);
    ggame_Config config = {.headless = true};
    ggame_Game *game = ggame_new(alloc, &config);
    int changed = GGAME_TEST_CHDIR("..");
    size_t size = 0;
    void *source = game && !changed ? gvfs_read(alloc, ggame_vfs(game), "/game.lua", &size) : NULL;
    int restored = GGAME_TEST_CHDIR(original);
    galloc_free(alloc, original);
    ggame_destroy(alloc, game);
    mewassert("restore working directory", restored == 0);
    mewassert("default mount stays anchored after chdir", changed == 0 && source && size > 0);
    galloc_free(alloc, source);
})

TEST(entity_destroy, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame_Config config = {.asset_root = ggame__test_asset_root, .initial_script = "/game.lua", .headless = true};
    ggame_Game *game = ggame_new(alloc, &config);
    mewassert("Game expectation failed", game);
    gecs_World *world = ggame_world(game);
    gecs_Component timer_type = gecs_Component_find(world, "gtime_Timer");
    gecs_Entity entity = gecs_Component_entity(world, timer_type, 0);
    gscript_Script *script = ggame_script(game);
    lua_State *lua = gscript_state(script);
    gscript_push_entity(alloc, lua, world, entity);
    lua_setglobal(lua, "game_entity");
    const char *destroy_script =
        "destroy_calls = 0; log_info = function() destroy_calls = destroy_calls + 1 end; "
        "game_entity:destroy(); assert(destroy_calls == 1); assert(not game_entity:valid())";
    mewassert(
        "Game expectation failed",
        gscript_run(alloc, script, destroy_script, strlen(destroy_script), "destroy test")
    );
    mewassert("Game expectation failed", ggame_frame(alloc, game, 0.1f));
    const char *verify_script = "assert(destroy_calls == 1)";
    mewassert(
        "Game expectation failed",
        gscript_run(alloc, script, verify_script, strlen(verify_script), "verify destroy")
    );
    int finalizer_spawn_result = 0;
    lua_pushlightuserdata(lua, &finalizer_spawn_result);
    lua_pushcclosure(lua, ggame__test_finalizer_result, 1);
    lua_setglobal(lua, "record_finalizer_result");
    const char *finalizer =
        "finalizer = setmetatable({}, {__gc = function() record_finalizer_result(pcall(spawn)) end})";
    mewassert("register Lua finalizer", gscript_run(alloc, script, finalizer, strlen(finalizer), "finalizer"));
    ggame_destroy(alloc, game);
    mewassert("Lua finalizer cannot create entities during shutdown", finalizer_spawn_result == -1);
})

mewtest_main(if (mew_test_argc != 2) {
    fprintf(stderr, "usage: %s ASSET_DIRECTORY\n", mew_test_argv[0]);
    return EXIT_FAILURE;
} ggame__test_asset_root = mew_test_argv[1]);

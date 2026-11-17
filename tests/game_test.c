#include <glint/config.h>
#include <glint/glint.h>
#include <glint/modules/audio/module.h>
#include <glint/modules/dim2/module.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/module.h>
#include <glint/modules/time/bindings.h>
#include <glint/modules/time/module.h>
#include <glint/modules/time/time.h>
#include <glint/modules/types/bindings.h>
#include <glint/modules/types/module.h>
#include <glint/modules/vfs/module.h>
#include <glint/unit/unit.h>

#include <lua.h>

#include <string.h>

#if GLINT_PLATFORM_WINDOWS
#include <direct.h>
#define GLINT_TEST_CHDIR _chdir
#else
#include <unistd.h>
#define GLINT_TEST_CHDIR chdir
#endif

typedef struct glint__TestCounts {
    size_t imported;
    size_t load;
    size_t update;
    size_t render;
    size_t unload;
    void* scratch;
    bool context_destroyed;
} glint__TestCounts;

typedef struct glint__TestLifetimeProbe {
    galloc_Allocator alloc;
    gvfs_Vfs* vfs;
    gscript_lua_Runtime* script;
    glint__TestCounts* counts;
} glint__TestLifetimeProbe;

static const char* glint__test_asset_root;

static bool glint__test_on_import(gmod_Modules* modules, gecs_World* world, void* user);
static void glint__test_destroy_probe(void* value);
static bool glint__test_on_load(gecs_World* world, void* user, char* error, size_t error_size);
static void glint__test_on_unload(gecs_World* world, void* user);
static bool glint__test_on_update(gecs_World* world, void* user, char* error, size_t error_size);
static bool glint__test_on_render(gecs_World* world, void* user, char* error, size_t error_size);
static int glint__test_finalizer_result(lua_State* lua);
static bool glint__test_can_unload(gecs_World* world, void* user);

typedef struct glint__GameContext {
    galloc_Allocator alloc;
    glint_Game* game;
    glint__TestCounts counts;
    gmod_ModuleConfigEntry value;
    gmod_Module module;
    char* original_directory;
    void* source;
    int finalizer_result;
    bool unload_allowed;
} glint__GameContext;

static bool glint__test_context_create(glint__GameContext* context, const glint_Config* config);
static bool glint__test_lifecycle_create(glint__GameContext* context);
static void glint__test_context_free(void* user);
static gecs_Entity glint__test_timer_entity(glint__GameContext* context);

GUNIT_TEST(headless_audio_import,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gmod_Module modules[] = {gaudio_module(), gaudio_module()};
    glint_Config config =
        {.asset_root = glint__test_asset_root, .headless = true, .modules = modules, .module_count = 2};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gecs_World* world = glint_world(context.game);
    gaudio_Service* service = gecs_context_get(world, GECS_CONTEXT(world, gaudio_Service));
    gunit_assert(
        "headless host remains unavailable",
        service != NULL && !service->device.open && service->module_owned
    );
    gunit_assert("headless game steps", glint_frame(context.alloc, context.game, 0.1f));
)

GUNIT_TEST(headless_dim2_import,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gmod_Module modules[] = {gdim2_module(), gdim2_module(), gtypes_module(), gtime_module()};
    glint_Config config =
        {.asset_root = glint__test_asset_root, .headless = true, .modules = modules, .module_count = 4};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gecs_World* world = glint_world(context.game);
    gdim2_Service* service = gecs_context_get(world, GECS_CONTEXT(world, gdim2_Service));
    gunit_assert("headless 2D module loaded without graphics", service != NULL && service->instance == NULL);
    gunit_assert("headless game steps", glint_frame(context.alloc, context.game, 0.1f));
)

GUNIT_TEST(game_imports_and_loads_custom_module,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("lifecycle create", glint__test_lifecycle_create(&context));
    gunit_assert("custom module imported once", context.counts.imported == 1);
    gunit_assert("custom module loaded once", context.counts.load == 1);
)

GUNIT_TEST(game_setup_creates_repeating_timer,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("lifecycle create", glint__test_lifecycle_create(&context));
    gecs_Entity entity = glint__test_timer_entity(&context);
    gunit_assert("timer entity is available", entity.world != NULL && entity.id != 0);
    gecs_Component type = gecs_Component_find(entity.world, "gtime_Timer");
    gtime_Timer* timer = gecs_Component_get(entity, type);
    gunit_assert(
        "setup configures repeating timer",
        timer != NULL && timer->duration == 2.0f && timer->mode == GTIME_TIMER_REPEATING
    );
)

GUNIT_TEST(game_frame_advances_timer,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("lifecycle create", glint__test_lifecycle_create(&context));
    gecs_Entity entity = glint__test_timer_entity(&context);
    gunit_assert("timer entity is available", entity.world != NULL && entity.id != 0);
    gecs_Component type = gecs_Component_find(entity.world, "gtime_Timer");
    gunit_assert("run first frame", glint_frame(context.alloc, context.game, 2.1f));
    gtime_Timer* timer = gecs_Component_get(entity, type);
    gunit_assert(
        "timer advances with frame delta",
        timer != NULL && timer->is_finished && timer->times_finished_this_tick == 1
    );
)

GUNIT_TEST(game_frames_update_and_render_modules,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("lifecycle create", glint__test_lifecycle_create(&context));
    gunit_assert("run first frame", glint_frame(context.alloc, context.game, 2.1f));
    gunit_assert("update and render run once", context.counts.update == 1 && context.counts.render == 1);
    gunit_assert("run second frame", glint_frame(context.alloc, context.game, 0.1f));
    gunit_assert("update and render run again", context.counts.update == 2 && context.counts.render == 2);
    gunit_assert("successful frame has no error", strcmp(glint_error(context.game), "") == 0);
)

GUNIT_TEST(game_shutdown_preserves_context_dependencies,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("lifecycle create", glint__test_lifecycle_create(&context));
    glint__test_context_free(&context);
    gunit_assert("custom module unloaded once", context.counts.unload == 1);
    gunit_assert("lifetime context destroyed while dependencies alive", context.counts.context_destroyed);
)

GUNIT_TEST(default_mount_snapshots_working_directory,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    context.alloc = galloc_malloc_allocator();
    context.original_directory = gvfs_get_working_directory(context.alloc);
    gunit_assert("get original working directory", context.original_directory != NULL);
    gunit_assert("enter asset directory", GLINT_TEST_CHDIR(glint__test_asset_root) == 0);
    glint_Config config = {.headless = true};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gunit_assert("leave asset directory", GLINT_TEST_CHDIR("..") == 0);
    size_t size = 0;
    context.source = gvfs_read(context.alloc, glint_vfs(context.game), "/game.lua", &size);
    gunit_assert("default mount stays anchored after chdir", context.source != NULL && size > 0);
    gunit_assert("restore working directory", GLINT_TEST_CHDIR(context.original_directory) == 0);
)

GUNIT_TEST(entity_destroy_runs_callback_once,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Config config = {.asset_root = glint__test_asset_root, .initial_script = "/game.lua", .headless = true};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gecs_Entity entity = glint__test_timer_entity(&context);
    gunit_assert("timer entity is available", entity.world != NULL && entity.id != 0);
    gscript_lua_Runtime* script = glint_script(context.game);
    lua_State* lua = gscript_lua_state(script);
    gscript_bindings_push_entity(lua, entity);
    lua_setglobal(lua, "game_entity");
    const char* source =
        "destroy_calls = 0; log_info = function() destroy_calls = destroy_calls + 1 end; "
        "game_entity:destroy(); assert(destroy_calls == 1); assert(not game_entity:valid())";
    gunit_assert(
        "run entity destruction script",
        gscript_lua_run(context.alloc, script, source, strlen(source), "destroy test")
    );
    gunit_assert("run frame after destruction", glint_frame(context.alloc, context.game, 0.1f));
    const char* verify = "assert(destroy_calls == 1)";
    gunit_assert(
        "destruction callback not repeated",
        gscript_lua_run(context.alloc, script, verify, strlen(verify), "verify destroy")
    );
)

GUNIT_TEST(shutdown_finalizer_cannot_spawn_entities,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Config config = {.asset_root = glint__test_asset_root, .initial_script = "/game.lua", .headless = true};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gscript_lua_Runtime* script = glint_script(context.game);
    lua_State* lua = gscript_lua_state(script);
    lua_pushlightuserdata(lua, &context.finalizer_result);
    lua_pushcclosure(lua, glint__test_finalizer_result, 1);
    lua_setglobal(lua, "record_finalizer_result");
    const char* source = "finalizer = setmetatable({}, {__gc = function() record_finalizer_result(pcall(spawn)) end})";
    gunit_assert("register Lua finalizer", gscript_lua_run(context.alloc, script, source, strlen(source), "finalizer"));
    glint__test_context_free(&context);
    gunit_assert("finalizer cannot create entities during shutdown", context.finalizer_result == -1);
)

GUNIT_TEST(game_destroy_retries_module_veto,
    glint__GameContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    context.module = (gmod_Module) {
        GMOD_INIT,
        .name = "test.veto",
        .user = &context.unload_allowed,
        .can_unload = glint__test_can_unload
    };
    glint_Config config =
        {.asset_root = glint__test_asset_root, .headless = true, .modules = &context.module, .module_count = 1};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    bool destroyed = glint_destroy(context.alloc, context.game);
    if (destroyed) context.game = NULL;
    gunit_assert("veto reports deferred destruction", !destroyed);
    gunit_assert("game stays usable", glint_frame(context.alloc, context.game, 0.1f));
    context.unload_allowed = true;
    destroyed = glint_destroy(context.alloc, context.game);
    if (destroyed) context.game = NULL;
    gunit_assert("destruction succeeds on retry", destroyed);
)

gunit_main(if (gunit_argc != 2) {
    fprintf(stderr, "usage: %s ASSET_DIRECTORY\n", gunit_argv[0]);
    return EXIT_FAILURE;
} glint__test_asset_root = gunit_argv[1]);

static bool glint__test_on_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)world;
    const gmod_ModuleConfig* config = gmod_Modules_config(modules, "test");
    const gmod_ModuleConfigEntry* entry = gmod_ModuleConfig_get(config, "message");
    if (entry == NULL || entry->kind != GMOD_STRING || strcmp(entry->value.string, "from game") != 0) {
        return false;
    }
    ((glint__TestCounts*)user)->imported += 1;
    return true;
}

static void glint__test_destroy_probe(void* value) {
    glint__TestLifetimeProbe* probe = value;
    size_t length = 0;
    void* source = gvfs_read(probe->alloc, probe->vfs, "/game.lua", &length);
    gunit_check("VFS must outlive world contexts", (source != NULL) && (length != 0));
    lua_State* lua = gscript_lua_state(probe->script);
    lua_getglobal(lua, "Timer");
    gunit_check("Lua state must outlive world contexts", lua_istable(lua, -1));
    lua_pop(lua, 1);
    galloc_free(probe->alloc, source);
    probe->counts->context_destroyed = true;
}

static bool glint__test_on_load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    galloc_Allocator alloc = glint_allocator(world);
    gvfsmod_Service* vfs = gecs_context_get(world, GECS_CONTEXT(world, gvfsmod_Service));
    gscript_Service* script = gecs_context_get(world, GECS_CONTEXT(world, gscript_Service));
    if (!gunit_check(
            "required services available",
            (vfs != NULL) && (vfs->vfs != NULL) && (script != NULL) && (script->script != NULL)
        )) {
        return false;
    }
    gecs_Component type =
        gecs_context_register(world, "LifetimeProbe", sizeof(glint__TestLifetimeProbe), glint__test_destroy_probe);
    glint__TestLifetimeProbe probe = {.alloc = alloc, .vfs = vfs->vfs, .script = script->script, .counts = user};
    if (!gunit_check(
            "Could not register lifetime probe",
            (type != 0) && (gecs_context_set(world, type, &probe) != NULL)
        )) {
        return false;
    }
    ((glint__TestCounts*)user)->load += 1;
    return true;
}

static void glint__test_on_unload(gecs_World* world, void* user) {
    gecs_Component type = gecs_context_find(world, "LifetimeProbe");
    if (type > 0) {
        gecs_context_remove(world, type);
    }
    ((glint__TestCounts*)user)->unload += 1;
}

static bool glint__test_on_update(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    glint__TestCounts* counts = user;
    void* scratch = galloc_alloc(glint_temp_allocator(world), 32);
    if (!gunit_check("allocate frame scratch", (scratch != NULL))) {
        return false;
    }
    counts->scratch = scratch;
    memset(scratch, 42, 32);
    counts->update += 1;
    return true;
}

static bool glint__test_on_render(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)world;
    glint__TestCounts* counts = user;
    if (!gunit_check("frame scratch is available during render", counts->scratch != NULL)) {
        return false;
    }
    for (size_t i = 0; i < 32; i++) {
        if (!gunit_check("scratch contents survive through render", ((uint8_t*)counts->scratch)[i] == 42)) {
            return false;
        }
    }
    counts->scratch = NULL;
    counts->render += 1;
    return true;
}

static int glint__test_finalizer_result(lua_State* lua) {
    int* result = lua_touserdata(lua, lua_upvalueindex(1));
    *result = lua_toboolean(lua, 1) != 0 ? 1 : -1;
    return 0;
}

static bool glint__test_can_unload(gecs_World* world, void* user) {
    (void)world;
    return *(bool*)user;
}

static bool glint__test_context_create(glint__GameContext* context, const glint_Config* config) {
    context->alloc = galloc_malloc_allocator();
    context->game = glint_new(context->alloc, config);
    if (!gunit_check("game created", context->game != NULL)) {
        return false;
    }

    return true;
}

static bool glint__test_lifecycle_create(glint__GameContext* context) {
    context->value = (gmod_ModuleConfigEntry) {.key = "message", .kind = GMOD_STRING, .value.string = "from game"};
    context->module = (gmod_Module) {
        GMOD_INIT,
        .name = "test",
        .user = &context->counts,
        .import = glint__test_on_import,
        .load = glint__test_on_load,
        .unload = glint__test_on_unload,
        .update = glint__test_on_update,
        .render = glint__test_on_render,
        .config = {.entries = &context->value, .count = 1}
    };
    glint_Config config = {
        .asset_root = glint__test_asset_root,
        .initial_script = "/game.lua",
        .modules = &context->module,
        .module_count = 1,
        .headless = true
    };
    if (!glint__test_context_create(context, &config)) {
        return false;
    }

    return true;
}

static void glint__test_context_free(void* user) {
    glint__GameContext* context = user;
    context->unload_allowed = true;
    if (context->original_directory != NULL) {
        int restored = GLINT_TEST_CHDIR(context->original_directory);
        gunit_check("cleanup restores working directory", restored == 0);
    }
    if (context->game != NULL) {
        glint_destroy(context->alloc, context->game);
        context->game = NULL;
    }
    galloc_free(context->alloc, context->source);
    context->source = NULL;
    galloc_free(context->alloc, context->original_directory);
    context->original_directory = NULL;
}

static gecs_Entity glint__test_timer_entity(glint__GameContext* context) {
    gecs_World* world = glint_world(context->game);
    gecs_Component type = gecs_Component_find(world, "gtime_Timer");
    if (!gunit_check("setup creates timer component", type != 0 && gecs_Component_count(world, type) == 1)) {
        return (gecs_Entity) {0};
    }
    return gecs_Component_entity(world, type, 0);
}

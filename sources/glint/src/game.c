#include <glint/game.h>
#include <glint/audio.h>
#include <glint/behaviors.h>
#include <glint/dim2/dim2.h>
#include <glint/time_lua.h>
#include <glint/types_lua.h>
#include <glint/window_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

struct ggame_Game {
    gecs_World *world;
    gmod_Modules *modules;
    gscript_Script *script;
    gvfs_Vfs *vfs;
    gdim2_Dim2 *dim2;
    gbeh_Behaviors *behaviors;
    bool window_open;
    gaudio_Device audio;
    bool headless;
    float delta_time;
    char error[512];
#ifdef __EMSCRIPTEN__
    galloc_Allocator loop_alloc;
#endif
};

#ifdef __EMSCRIPTEN__
/* Emscripten supports one main loop per thread. Game operations, including
 * destruction, must take place on the thread that registered the loop. */
static ggame_Game *ggame__web_loop_game;

static void ggame__game_game_cancel_loop(ggame_Game *game) {
    if (ggame__web_loop_game == game) {
        emscripten_cancel_main_loop();
        ggame__web_loop_game = NULL;
    }
}

static void ggame__game_game_web_frame(void *arg) {
    ggame_Game *game = arg;
    if (WindowShouldClose()) {
        ggame__game_game_cancel_loop(game);
    } else if (!ggame_frame(game->loop_alloc, game, GetFrameTime())) {
        fprintf(stderr, "glint: %s\n", ggame_error(game));
        ggame__game_game_cancel_loop(game);
    }
}
#endif

static void ggame__game_game_error(ggame_Game *game, const char *message) {
    snprintf(game->error, sizeof(game->error), "%s", message ? message : "unknown error");
}

static void ggame__game_lua_error_message(ggame_Game *game) {
    lua_State *lua = gscript_state(game->script);
    const char *message = lua_tostring(lua, -1);
    ggame__game_game_error(game, message ? message : "Lua error");
    lua_pop(lua, 1);
}

static float ggame__game_game_delta_time(void *user) {
    ggame_Game *game = user;
    return game->delta_time;
}

typedef struct ggame__BindTimeArgs {
    galloc_Allocator alloc;
    ggame_Game *game;
} ggame__BindTimeArgs;

static int ggame__game_bind_time_protected(lua_State *lua) {
    ggame__BindTimeArgs *args = lua_touserdata(lua, 1);
    if (!gwindow_bind_lua(args->alloc, lua, ggame__game_game_delta_time, args->game)) {
        return luaL_error(lua, "could not register window bindings");
    }
    return 0;
}

static bool ggame__game_bind_time(galloc_Allocator alloc, ggame_Game *game) {
    lua_State *lua = gscript_state(game->script);
    ggame__BindTimeArgs args = {.alloc = alloc, .game = game};
    lua_pushcfunction(lua, ggame__game_bind_time_protected);
    lua_pushlightuserdata(lua, &args);
    if (lua_pcall(lua, 1, 0, 0) != LUA_OK) {
        ggame__game_lua_error_message(game);
        return false;
    }
    return true;
}

static bool ggame__game_start_behavior(galloc_Allocator alloc, ggame_Game *game, const char *path) {
    gecs_Entity entity = gecs_Entity_new(alloc, game->world);
    if (!entity) {
        ggame__game_game_error(game, "could not create initial entity");
        return false;
    }
    if (!gbeh_attach(alloc, game->behaviors, entity, path)) {
        ggame__game_lua_error_message(game);
        gecs_Entity_destroy(game->world, entity);
        return false;
    }
    return true;
}

ggame_Game *ggame_new(galloc_Allocator alloc, const ggame_Config *config) {
    if (!config || (config->module_count && !config->modules)
        || (config->mount_count && !config->mounts) || config->log_level > GLOG_NONE) {
        return NULL;
    }
    if (config->log_level != GLOG_DEFAULT) {
        glog_set_level(config->log_level);
    }
    ggame_Game *game = galloc_calloc(alloc, 1, sizeof(*game));
    if (!game) {
        return NULL;
    }
    game->headless = config->headless;
    if (!game->headless) {
        unsigned flags = 0;
        if (config->vsync) {
            flags |= FLAG_VSYNC_HINT;
        }
        if (config->resizable) {
            flags |= FLAG_WINDOW_RESIZABLE;
        }
        if (flags) {
            SetConfigFlags(flags);
        }
        InitWindow(
            config->width > 0 ? config->width : 800,
            config->height > 0 ? config->height : 600,
            config->title ? config->title : "glint"
        );
        game->window_open = IsWindowReady();
        if (!game->window_open) {
            ggame__game_game_error(game, "could not initialize window");
            goto fail;
        }
        SetTargetFPS(config->fps > 0 ? config->fps : 0);
        (void)gaudio_Device_open(alloc, &game->audio, 0);
    }
    game->world = gecs_World_new(alloc);
    if (!game->world) {
        ggame__game_game_error(game, "could not create world");
        goto fail;
    }
    /* Publish the game-owned device, including its unavailable headless state,
     * so an explicit audio module import never opens or closes it again. */
    gecs_Component audio_type = GECS_CONTEXT(alloc, game->world, gaudio_Service);
    gaudio_Service audio_service = {.device = game->audio};
    if (!audio_type || !gecs_context_set(alloc, game->world, audio_type, &audio_service)) {
        ggame__game_game_error(game, "could not register audio service");
        goto fail;
    }
    game->vfs = gvfs_create(alloc);
    game->modules = gmod_Modules_new(alloc, game->world);
    game->script = gscript_new(alloc);
    if (!game->vfs || !game->modules || !game->script) {
        ggame__game_game_error(game, "could not initialize game modules");
        goto fail;
    }
    char *working_directory = NULL;
    if (!config->asset_root) {
        working_directory = gvfs_get_working_directory(alloc);
        if (!working_directory) {
            ggame__game_game_error(game, "could not get working directory");
            goto fail;
        }
    }
    bool mounted = gvfs_mount(alloc, game->vfs, "/", config->asset_root ? config->asset_root : working_directory);
    galloc_free(alloc, working_directory);
    if (!mounted) {
        ggame__game_game_error(game, "could not mount asset directory");
        goto fail;
    }
    for (size_t i = 0; i < config->mount_count; i++) {
        const ggame_Mount *mount = &config->mounts[i];
        if (!mount->point || !mount->root
            || !gvfs_mount(alloc, game->vfs, mount->point, mount->root)) {
            ggame__game_game_error(game, "could not mount asset directory");
            goto fail;
        }
    }
    if (!gscript_bind_world(alloc, game->script, game->world)) {
        ggame__game_lua_error_message(game);
        goto fail;
    }
    game->behaviors = gbeh_new(alloc, game->script, game->world, game->vfs);
    if (!game->behaviors) {
        ggame__game_game_error(game, "could not initialize behaviors");
        goto fail;
    }
    if (!ggame__game_bind_time(alloc, game)) {
        goto fail;
    }
    gecs_Component services_type = GECS_CONTEXT(alloc, game->world, ggame_Services);
    ggame_Services services = {.vfs = game->vfs, .script = game->script};
    if (!services_type || !gecs_context_set(alloc, game->world, services_type, &services)) {
        ggame__game_game_error(game, "could not register game services");
        goto fail;
    }
    gecs_Component lua_type = GECS_CONTEXT(alloc, game->world, gtypes_LuaService);
    gtypes_LuaService lua_service = {.lua = gscript_state(game->script)};
    if (!lua_type || !gecs_context_set(alloc, game->world, lua_type, &lua_service)) {
        ggame__game_game_error(game, "could not register Lua service");
        goto fail;
    }
    gecs_Component time_type = GECS_CONTEXT(alloc, game->world, gtime_Host);
    gtime_Host time_host = {.frame_time = ggame__game_game_delta_time, .user = game};
    if (!time_type || !gecs_context_set(alloc, game->world, time_type, &time_host)) {
        ggame__game_game_error(game, "could not register time service");
        goto fail;
    }
    gecs_Component dim2_host_type = GECS_CONTEXT(alloc, game->world, gdim2_Host);
    gdim2_Host dim2_host = {.vfs = game->vfs, .script = game->script,
                               .delta_time = &game->delta_time, .enabled = !game->headless,
                               .defer_destroy = true};
    if (!dim2_host_type || !gecs_context_set(alloc, game->world, dim2_host_type, &dim2_host)) {
        ggame__game_game_error(game, "could not register 2D services");
        goto fail;
    }
    gmod_Module dim2_module = gdim2_module();
    gmod_Module types_module = gtypes_module();
    gmod_Module time_module = gtime_module();
    int lua_top = lua_gettop(gscript_state(game->script));
    if (!gmod_Modules_import(alloc, game->modules, &types_module) ||
        !gmod_Modules_import(alloc, game->modules, &time_module) ||
        !gmod_Modules_import(alloc, game->modules, &dim2_module) ||
        !gmod_Modules_load(alloc, game->modules)) {
        if (lua_gettop(gscript_state(game->script)) > lua_top) ggame__game_lua_error_message(game);
        else ggame__game_game_error(game, "could not initialize 2D module");
        goto fail;
    }
    gecs_Component dim2_service_type = GECS_CONTEXT(alloc, game->world, gdim2_Service);
    gdim2_Service *dim2_service = gecs_context_get(game->world, dim2_service_type);
    game->dim2 = dim2_service ? dim2_service->instance : NULL;
    for (size_t i = 0; i < config->module_count; i++) {
        if (!gmod_Modules_import_active(alloc, game->modules, &config->modules[i])) {
            ggame__game_game_error(game, "could not import module");
            goto fail;
        }
    }
    if (!gmod_Modules_load_pending(alloc, game->modules)) {
        ggame__game_game_error(game, "could not load modules");
        goto fail;
    }
    if (config->initial_script && !ggame__game_start_behavior(alloc, game, config->initial_script)) {
        goto fail;
    }
    return game;

fail:
    fprintf(stderr, "glint: %s\n", game->error);
    ggame_destroy(alloc, game);
    return NULL;
}

void ggame_destroy(galloc_Allocator alloc, ggame_Game *game) {
    if (!game) {
        return;
    }
#ifdef __EMSCRIPTEN__
    ggame__game_game_cancel_loop(game);
#endif
    if (game->modules) {
        gmod_Modules_unload_except(alloc, game->modules, GDIM2_MODULE_NAME);
    }
    gbeh_destroy(alloc, game->behaviors);
    if (game->modules) {
        gmod_Modules_unload(alloc, game->modules);
    }
    if (game->world) {
        gecs_World_clear(alloc, game->world);
        gecs_World_freeze(game->world);
    }
    if (game->modules) {
        gmod_Modules_free(alloc, game->modules);
    }
    if (game->script) {
        gscript_destroy(alloc, game->script);
    }
    gdim2_destroy(alloc, game->dim2);
    gecs_World_free(alloc, game->world);
    gvfs_destroy(alloc, game->vfs);
    gaudio_Device_close(&game->audio);
    if (game->window_open) {
        CloseWindow();
    }
    galloc_free(alloc, game);
}

bool ggame_frame(galloc_Allocator alloc, ggame_Game *game, float delta_time) {
    if (!game) {
        return false;
    }
    game->error[0] = '\0';
    game->delta_time = delta_time;
    gmod_Modules_update_named(alloc, game->modules, GDIM2_MODULE_NAME, false);
    bool ok = gbeh_update(alloc, game->behaviors);
    if (!ok) {
        ggame__game_lua_error_message(game);
    }
    if (ok) {
        gmod_Modules_update_named(alloc, game->modules, GDIM2_MODULE_NAME, true);
        if (!game->headless) {
            BeginDrawing();
            ClearBackground(BLACK);
        }
        gmod_Modules_render_named(alloc, game->modules, GDIM2_MODULE_NAME, true);
        gmod_Modules_render_named(alloc, game->modules, GDIM2_MODULE_NAME, false);
        if (!game->headless) {
            EndDrawing();
        }
    }
    gmod_Modules_end_frame(game->modules);
    return ok;
}

const char *ggame_error(const ggame_Game *game) {
    return game ? game->error : "invalid game";
}

gecs_World *ggame_world(ggame_Game *game) {
    return game ? game->world : NULL;
}

gscript_Script *ggame_script(ggame_Game *game) {
    return game ? game->script : NULL;
}

gvfs_Vfs *ggame_vfs(ggame_Game *game) {
    return game ? game->vfs : NULL;
}

bool ggame_run(galloc_Allocator alloc, ggame_Game *game) {
    if (!game || game->headless || !game->window_open) {
        return false;
    }
#ifdef __EMSCRIPTEN__
    if (ggame__web_loop_game) {
        ggame__game_game_error(game, "a game loop is already running");
        return false;
    }
    game->error[0] = '\0';
    game->loop_alloc = alloc;
    ggame__web_loop_game = game;
    emscripten_set_main_loop_arg(ggame__game_game_web_frame, game, 0, false);
    return true;
#else
    while (!WindowShouldClose()) {
        if (!ggame_frame(alloc, game, GetFrameTime())) {
            return false;
        }
    }
    return true;
#endif
}

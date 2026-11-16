#include <glint/glint.h>
#include <glint/modules/window/module.h>
#include <glint/platform/rendering.h>
#include <glint/platform/windowing.h>

#include <stdio.h>
#include <string.h>

struct glint_Game {
    gecs_World* world;
    gmod_Modules* modules;
    bool headless;
    char error[512];
};

typedef struct glint__RunState {
    galloc_Allocator alloc;
    glint_RunEndFn end;
    void* user;
} glint__RunState;

static void glint__game_game_error(glint_Game* game, const char* message);
static glint__RunState* glint__game_run_state(glint_Game* game);
static bool glint__game_run_frame(void* user, float delta);
static void glint__game_run_end(void* user, bool ok);

glint_Game* glint_new(galloc_Allocator alloc, const glint_Config* config) {
    if (config == NULL) {
        return NULL;
    }

    bool missing_modules = config->module_count > 0 && config->modules == NULL;
    bool missing_mounts = config->mount_count > 0 && config->mounts == NULL;
    bool log_level_valid = config->log_level >= GLOG_DEFAULT && config->log_level <= GLOG_FATAL;
    if (missing_modules || missing_mounts || !log_level_valid) {
        return NULL;
    }

    glint_Game* game = galloc_calloc(alloc, 1, sizeof(*game));
    if (game == NULL) {
        return NULL;
    }

    game->headless = config->headless;
    game->world = gecs_World_new(alloc);
    if (game->world == NULL) {
        glint__game_game_error(game, "could not create world");
        goto fail;
    }

    gecs_Component config_type = GECS_CONTEXT(game->world, glint_Config);
    gecs_Component clock_type = GECS_CONTEXT(game->world, gtime_FrameTime);
    gtime_FrameTime clock = {0};
    bool contexts_registered = config_type != 0 && clock_type != 0;
    bool contexts_set = contexts_registered && gecs_context_set(game->world, config_type, config) != NULL
        && gecs_context_set(game->world, clock_type, &clock) != NULL;
    if (!contexts_set) {
        glint__game_game_error(game, "could not register game configuration");
        goto fail;
    }

    game->modules = gmod_Modules_new(game->world);
    if (game->modules == NULL) {
        glint__game_game_error(game, "could not initialize game modules");
        goto fail;
    }

    if (!gmod_Modules_import(alloc, game->modules, gcore_module())) {
        glint__game_game_error(game, "could not import default modules");
        goto fail;
    }

    for (size_t i = 0; i < config->module_count; i++) {
        if (!gmod_Modules_import(alloc, game->modules, config->modules[i])) {
            glint__game_game_error(game, "could not import module");
            goto fail;
        }
    }

    if (!gmod_Modules_load(alloc, game->modules)) {
        glint__game_game_error(game, gmod_Modules_error(game->modules));
        goto fail;
    }

    gecs_context_remove(game->world, config_type);
    return game;

fail:
    fprintf(stderr, "glint: %s\n", game->error);
    glint_destroy(alloc, game);
    return NULL;
}

bool glint_destroy(galloc_Allocator alloc, glint_Game* game) {
    if (game == NULL) {
        return true;
    }

    if (game->modules != NULL && !gmod_Modules_shutdown(alloc, game->modules)) {
        return false;
    }

    gplatform_cancel_loop(game);
    if (game->modules != NULL) {
        if (!gmod_Modules_free(alloc, game->modules)) {
            return false;
        }

        game->modules = NULL;
    }

    gecs_World_free(game->world);
    galloc_free(alloc, game);
    return true;
}

bool glint_frame(galloc_Allocator alloc, glint_Game* game, float delta_time) {
    if (game == NULL) {
        return false;
    }

    game->error[0] = '\0';
    gecs_Component clock_type = gecs_context_find(game->world, "gtime_FrameTime");
    gtime_FrameTime* clock = clock_type != 0 ? gecs_context_get(game->world, clock_type) : NULL;
    if (clock != NULL) {
        clock->delta = delta_time;
    }

    bool ok = gmod_Modules_update(alloc, game->modules);

    if (ok) {
        if (!game->headless) {
            gplatform_begin_drawing();
            gplatform_clear_background((gcolor_Color) {0, 0, 0, 255});
        }

        ok = gmod_Modules_render(alloc, game->modules);
        if (!game->headless) {
            gplatform_end_drawing();
        }
    }

    if (!ok) {
        glint__game_game_error(game, gmod_Modules_error(game->modules));
    }

    gmod_Modules_end_frame(game->modules);
    return ok;
}

const char* glint_error(const glint_Game* game) {
    return game != NULL ? game->error : "invalid game";
}

gecs_World* glint_world(glint_Game* game) {
    return game != NULL ? game->world : NULL;
}

gscript_lua_Runtime* glint_script(glint_Game* game) {
    if (game == NULL || game->world == NULL) {
        return NULL;
    }

    gecs_Component type = gecs_context_find(game->world, "gscript_Service");
    gscript_Service* service = type != 0 ? gecs_context_get(game->world, type) : NULL;
    return service != NULL ? service->script : NULL;
}

gvfs_Vfs* glint_vfs(glint_Game* game) {
    if (game == NULL || game->world == NULL) {
        return NULL;
    }

    gecs_Component type = gecs_context_find(game->world, "gvfsmod_Service");
    gvfsmod_Service* service = type != 0 ? gecs_context_get(game->world, type) : NULL;
    return service != NULL ? service->vfs : NULL;
}

bool glint_run_with_completion(galloc_Allocator alloc, glint_Game* game, glint_RunEndFn end, void* user) {
    if (game == NULL) {
        return false;
    }

    gecs_Component window_type = GECS_CONTEXT(game->world, gwindow_Service);
    const gwindow_Service* window = window_type != 0 ? gecs_context_get(game->world, window_type) : NULL;
    bool window_ready = window != NULL && window->open;
    bool loop_available = !game->headless && window_ready && !gplatform_loop_running();
    if (!loop_available) {
        glint__game_game_error(game, "could not start game loop");
        if (end != NULL) {
            end(alloc, game, false, user);
        }

        return false;
    }

    gecs_Component state_type = GECS_CONTEXT(game->world, glint__RunState);
    glint__RunState state = {.alloc = alloc, .end = end, .user = user};
    if (state_type == 0 || gecs_context_set(game->world, state_type, &state) == NULL) {
        glint__game_game_error(game, "could not initialize game loop");
        if (end != NULL) {
            end(alloc, game, false, user);
        }

        return false;
    }

    game->error[0] = '\0';
    return gplatform_run_loop(glint__game_run_frame, glint__game_run_end, game);
}

bool glint_run(galloc_Allocator alloc, glint_Game* game) {
    return glint_run_with_completion(alloc, game, NULL, NULL);
}

static void glint__game_game_error(glint_Game* game, const char* message) {
    snprintf(game->error, sizeof(game->error), "%s", message != NULL ? message : "unknown error");
}

static glint__RunState* glint__game_run_state(glint_Game* game) {
    gecs_Component type = gecs_context_find(game->world, "glint__RunState");
    return type != 0 ? gecs_context_get(game->world, type) : NULL;
}

static bool glint__game_run_frame(void* user, float delta) {
    glint_Game* game = user;
    glint__RunState* state = glint__game_run_state(game);
    return state != NULL && glint_frame(state->alloc, game, delta);
}

static void glint__game_run_end(void* user, bool ok) {
    glint_Game* game = user;
    glint__RunState* state = glint__game_run_state(game);
    if (state != NULL && state->end != NULL) {
        state->end(state->alloc, game, ok, state->user);
    }
}

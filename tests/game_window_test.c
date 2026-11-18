#include <glint/glint.h>
#include <glint/unit/unit.h>

#include <raylib.h>

#ifdef GLINT_TEST_WEB
#include <emscripten.h>
#include <lua.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Raylib stand-ins exercise window lifecycle without a graphics session.

typedef struct glint__RaylibState {
    bool window_ready;
    bool audio_ready;
    size_t audio_opens;
    size_t audio_closes;
    bool audio_closed_with_window;
    int init_width;
    int init_height;
    char init_title[64];
    unsigned int config_flags;
    int target_fps;
    TraceLogCallback raylib_log;
    int trace_level;
    size_t polls;
    size_t close_calls;
    size_t begins;
    size_t ends;
    size_t clears;
#ifdef GLINT_TEST_PLATFORM
    glog_Level log_level;
    char log_message[128];
    size_t log_count;
#endif
#ifdef GLINT_TEST_WEB
    void (*scheduled_frame)(void*);
    void* scheduled_arg;
    size_t cancellations;
#endif
} glint__RaylibState;

typedef struct glint__WindowContext {
    galloc_Allocator alloc;
    glint_Game* game;
    glint__RaylibState state;
    void* source;
    bool completed;
    int32_t allocator_state;
} glint__WindowContext;

static glint__RaylibState* glint__test_state;
static const char* glint__test_asset_root;
static const char* glint__test_extra_root;

static bool glint__test_context_create(glint__WindowContext* context, const glint_Config* config);
static void glint__test_context_free(void* user);
#ifndef GLINT_TEST_WEB
static bool glint__test_window_create(glint__WindowContext* context);
#endif
#ifdef GLINT_TEST_WEB
static bool glint__test_web_create(glint__WindowContext* context);
#endif
#ifdef GLINT_TEST_PLATFORM
static void glint__test_capture_log(glog_Level level, const char* format, va_list args, void* user);
#endif

static void glint__test_reset_raylib(void);
#ifdef GLINT_TEST_WEB
static bool glint__test_browser_tick(void);
static bool glint__test_check_loop_allocator(gecs_World* world, void* user, char* error, size_t error_size);
static void glint__test_end_and_destroy(galloc_Allocator alloc, glint_Game* game, bool ok, void* user);
static bool glint__test_fail_frame(gecs_World* world, void* user, char* error, size_t error_size);
static void glint__test_failed_and_destroy(galloc_Allocator alloc, glint_Game* game, bool ok, void* user);
static void glint__test_record_completion(galloc_Allocator alloc, glint_Game* game, bool ok, void* user);
#endif

#ifdef GLINT_TEST_WEB
void emscripten_set_main_loop_arg(void (*callback)(void*), void* arg, int fps, int simulate_infinite_loop) {
    (void)fps;
    if (!gunit_check("browser loop registration returns to its caller", simulate_infinite_loop == 0)) {
        return;
    }
    if (!gunit_check("browser-driven asynchronous loop", glint__test_state->scheduled_frame == NULL)) {
        return;
    }
    glint__test_state->scheduled_frame = callback;
    glint__test_state->scheduled_arg = arg;
}

void emscripten_cancel_main_loop(void) {
    if (!gunit_check("cancel only a registered loop", glint__test_state->scheduled_frame != NULL)) {
        return;
    }
    glint__test_state->scheduled_frame = NULL;
    glint__test_state->scheduled_arg = NULL;
    glint__test_state->cancellations += 1;
}

#endif

void InitWindow(int width, int height, const char* title) {
    glint__test_state->init_width = width;
    glint__test_state->init_height = height;
    snprintf(
        glint__test_state->init_title,
        sizeof(glint__test_state->init_title),
        "%s",
        title != NULL ? title : ""
    );
    glint__test_state->window_ready = true;
}

bool IsWindowReady(void) {
    return glint__test_state->window_ready;
}

void CloseWindow(void) {
    glint__test_state->window_ready = false;
    glint__test_state->close_calls += 1;
}

void SetConfigFlags(unsigned int flags) {
    glint__test_state->config_flags |= flags;
}

void SetTargetFPS(int fps) {
    glint__test_state->target_fps = fps;
}

void SetTraceLogLevel(int level) {
    glint__test_state->trace_level = level;
}

void SetTraceLogCallback(TraceLogCallback callback) {
    glint__test_state->raylib_log = callback;
}

bool WindowShouldClose(void) {
    glint__test_state->polls += 1;
    return glint__test_state->polls > 3;
}

float GetFrameTime(void) {
    return 1.0f / 60.0f;
}

double GetTime(void) {
    return 0.0;
}

int GetScreenWidth(void) {
    return glint__test_state->init_width;
}

int GetScreenHeight(void) {
    return glint__test_state->init_height;
}

void BeginDrawing(void) {
    glint__test_state->begins += 1;
}

void EndDrawing(void) {
    glint__test_state->ends += 1;
}

void ClearBackground(Color color) {
    (void)color;
    glint__test_state->clears += 1;
}

void TraceLog(int level, const char* format, ...) {
    if (glint__test_state->raylib_log == NULL || level < glint__test_state->trace_level) {
        return;
    }

    va_list args;
    va_start(args, format);
    glint__test_state->raylib_log(level, format, args);
    va_end(args);
}

bool IsAudioDeviceReady(void) {
    return glint__test_state->audio_ready;
}

void SetAudioStreamBufferSizeDefault(int size) {
    (void)size;
}

void InitAudioDevice(void) {
    glint__test_state->audio_ready = true;
    glint__test_state->audio_opens += 1;
}

void CloseAudioDevice(void) {
    glint__test_state->audio_closed_with_window = glint__test_state->window_ready;
    glint__test_state->audio_ready = false;
    glint__test_state->audio_closes += 1;
}

Image LoadImageFromMemory(const char* type, const unsigned char* bytes, int size) {
    (void)type;
    (void)bytes;
    (void)size;
    return (Image) {0};
}

bool IsImageValid(Image image) {
    return image.data != NULL;
}

void UnloadImage(Image image) {
    (void)image;
}

Texture2D LoadTextureFromImage(Image image) {
    (void)image;
    return (Texture2D) {0};
}

bool IsTextureValid(Texture2D texture) {
    return texture.id != 0;
}

void UnloadTexture(Texture2D texture) {
    (void)texture;
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) {
    (void)texture;
    (void)source;
    (void)dest;
    (void)origin;
    (void)rotation;
    (void)tint;
}

#ifndef GLINT_TEST_WEB
GUNIT_TEST(windowed_game,
    glint__WindowContext context = {0};

    GUNIT_SETUP({
        gunit_defer(glint__test_context_free, &context);
        gunit_assert("create windowed game", glint__test_window_create(&context));
    })

    GUNIT_SECTION("creation applies window configuration", {
        gunit_assert("window dimensions applied", context.state.init_width == 640 && context.state.init_height == 480);
        gunit_assert("window title applied", strcmp(context.state.init_title, "windowed") == 0);
        gunit_assert("vsync is enabled", (context.state.config_flags & FLAG_VSYNC_HINT) != 0);
        gunit_assert("window is resizable", (context.state.config_flags & FLAG_WINDOW_RESIZABLE) != 0);
        gunit_assert("target frame rate applied", context.state.target_fps == 30);
        gunit_assert("log level applied", glog_get_level() == GLOG_ERROR);
    })

    GUNIT_SECTION("creation opens the audio device once", {
        gecs_World* world = glint_world(context.game);
        const gaudio_Service* audio = gecs_context_get(world, GECS_CONTEXT(world, gaudio_Service));

        gunit_assert("module provides audio service", audio != NULL);
        gunit_assert("audio module owns a ready device", audio->module_owned && gaudio_Device_ready(&audio->device));
        gunit_assert("audio device opened once", context.state.audio_opens == 1);
    })

    GUNIT_SECTION("a manual frame draws once", {
        gunit_assert("run windowed frame", glint_frame(context.alloc, context.game, 0.016f));

        gunit_assert("drawing begins and ends once", context.state.begins == 1 && context.state.ends == 1);
        gunit_assert("background is cleared once", context.state.clears == 1);
    })

    GUNIT_SECTION("the native loop draws until the close request", {
        gunit_assert("run native loop", glint_run(context.alloc, context.game));

        gunit_assert("loop polls through the close request", context.state.polls == 4);
        gunit_assert("loop draws three complete frames", context.state.begins == 3 && context.state.ends == 3);
    })
)

GUNIT_TEST(window_shutdown_closes_audio_first,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("create windowed game", glint__test_window_create(&context));

    gunit_assert("game destruction succeeds", glint_destroy(context.alloc, context.game));
    context.game = NULL;

    gunit_assert("audio device closes once", context.state.audio_closes == 1);
    gunit_assert("audio device closes while the window is still open", context.state.audio_closed_with_window);
    gunit_assert("window closes once", context.state.close_calls == 1 && !context.state.window_ready);
)

#else
GUNIT_TEST(web_game_loop,
    glint__WindowContext context = {0};

    GUNIT_SETUP({
        gunit_defer(glint__test_context_free, &context);
        gunit_assert("create browser game", glint__test_web_create(&context));
    })

    GUNIT_SECTION("scheduling returns before drawing and rejects a second start", {
        gunit_assert("schedule game", glint_run(context.alloc, context.game));
        gunit_assert("loop is scheduled", context.state.scheduled_frame != NULL);
        gunit_assert("no frames are drawn during scheduling", context.state.begins == 0);
        gunit_assert("duplicate scheduling is rejected", !glint_run(context.alloc, context.game));

        gunit_assert("browser tick", glint__test_browser_tick());
        gunit_assert("scheduled frame draws", context.state.begins == 1 && context.state.ends == 1);
    })

    GUNIT_SECTION("normal completion can destroy the game", {
        gunit_assert(
            "schedule game",
            glint_run_with_completion(context.alloc, context.game, glint__test_end_and_destroy, &context)
        );
        for (size_t poll = 0; poll < 4; poll++) {
            gunit_assert("browser tick through the close request", glint__test_browser_tick());
        }
        gunit_assert(
            "completion destroys game",
            context.completed && context.game == NULL && context.state.close_calls == 1
        );
    })

    GUNIT_SECTION("destruction cancels the loop without invoking completion", {
        gunit_assert(
            "schedule game with completion",
            glint_run_with_completion(context.alloc, context.game, glint__test_record_completion, &context)
        );
        gunit_assert("loop awaits browser frames", context.state.scheduled_frame != NULL && !context.completed);
        gunit_assert("browser tick", glint__test_browser_tick());
        gunit_assert("loop remains scheduled after first tick", context.state.scheduled_frame != NULL);
        gunit_assert("completion has not run", !context.completed);

        gunit_assert("destroy active game", glint_destroy(context.alloc, context.game));
        context.game = NULL;

        gunit_assert("destroy cancels loop", context.state.scheduled_frame == NULL && context.state.cancellations == 1);
        gunit_assert("destroy never invokes completion", !context.completed);
    })
)

GUNIT_TEST(web_loop_failure_completion,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    const gmod_Module failure = {GMOD_INIT, .name = "failure", .update = glint__test_fail_frame};
    const glint_Config config = {.asset_root = glint__test_asset_root, .modules = &failure, .module_count = 1};
    gunit_assert("create browser game", glint__test_context_create(&context, &config));

    gunit_assert(
        "schedule failing game",
        glint_run_with_completion(context.alloc, context.game, glint__test_failed_and_destroy, &context)
    );
    gunit_assert("browser tick", glint__test_browser_tick());
    gunit_assert(
        "failure completes and destroys game",
        context.completed && context.game == NULL && context.state.close_calls == 1
    );
)

GUNIT_TEST(web_loop_preserves_allocator_state,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    context.alloc = galloc_malloc_allocator();
    context.alloc.data = &context.allocator_state;
    const gmod_Module module = {
        GMOD_INIT,
        .name = "allocator",
        .user = &context.allocator_state,
        .update = glint__test_check_loop_allocator
    };
    const glint_Config config = {.asset_root = glint__test_asset_root, .modules = &module, .module_count = 1};
    gunit_assert("create browser game", glint__test_context_create(&context, &config));

    gunit_assert("schedule allocator game", glint_run(context.alloc, context.game));
    gunit_assert("browser tick", glint__test_browser_tick());
    gunit_assert("allocator check ran in scheduled frame", context.state.begins == 1 && context.state.ends == 1);
)

GUNIT_TEST(web_game_after_window_close_request,
    glint__WindowContext context = {0};

    GUNIT_SETUP({
        gunit_defer(glint__test_context_free, &context);
        gunit_assert("create browser game", glint__test_web_create(&context));
        gunit_assert("schedule game", glint_run(context.alloc, context.game));
        for (size_t poll = 0; poll < 4; poll++) {
            gunit_assert("browser tick through the close request", glint__test_browser_tick());
        }
    })

    GUNIT_SECTION("the loop stops but leaves the game alive", {
        gunit_assert(
            "window close cancels loop",
            context.state.scheduled_frame == NULL && context.state.cancellations == 1
        );
        gunit_assert("window close does not destroy game", context.state.close_calls == 0);
    })

    GUNIT_SECTION("the game can restart the loop and cancel it on destruction", {
        gunit_assert("restart after close request", glint_run(context.alloc, context.game));

        gunit_assert("destroy restarted game", glint_destroy(context.alloc, context.game));
        context.game = NULL;

        gunit_assert("destroy cancels restarted loop", context.state.scheduled_frame == NULL);
        gunit_assert("loop was cancelled once per run", context.state.cancellations == 2);
        gunit_assert("window closes once", context.state.close_calls == 1);
    })
)

GUNIT_TEST(web_script_error_keeps_loop_running,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    const glint_Config config = {.asset_root = glint__test_asset_root, .initial_script = "/game.lua"};
    gunit_assert("create browser game", glint__test_context_create(&context, &config));

    lua_State* lua = gscript_lua_state(glint_script(context.game));
    lua_pushnil(lua);
    lua_setglobal(lua, "Timer");
    gunit_assert("schedule game", glint_run(context.alloc, context.game));
    gunit_assert("browser tick", glint__test_browser_tick());
    gunit_assert(
        "script callback failure keeps browser loop running",
        context.state.scheduled_frame != NULL && context.state.cancellations == 0 && context.state.begins == 1
    );
    glint__test_context_free(&context);
    gunit_assert("destroy cancels loop", context.state.cancellations == 1 && context.state.close_calls == 1);
)

#endif

#ifdef GLINT_TEST_PLATFORM
GUNIT_TEST(headless_game,
    glint__WindowContext context = {0};

    GUNIT_SETUP({
        gunit_defer(glint__test_context_free, &context);
        const glint_Config config = {
            .asset_root = glint__test_asset_root,
            .initial_script = "/game.lua",
            .log_level = GLOG_WARN,
            .headless = true
        };
        gunit_assert("create headless game", glint__test_context_create(&context, &config));
    })

    GUNIT_SECTION("creation applies the log level without configuring a window", {
        gunit_assert(
            "headless skips window configuration",
            context.state.init_width == 0 && context.state.config_flags == 0 && context.state.target_fps == 0
        );
        gunit_assert("headless log level applied", glog_get_level() == GLOG_WARN);
    })

    GUNIT_SECTION("frame and shutdown leave the window untouched and the loop is unavailable", {
        gunit_assert("headless frame succeeds", glint_frame(context.alloc, context.game, 0.016f));
        gunit_assert("drawing never begins", context.state.begins == 0);
        gunit_assert("headless game cannot run window loop", !glint_run(context.alloc, context.game));

        gunit_assert("destroy headless game", glint_destroy(context.alloc, context.game));
        context.game = NULL;

        gunit_assert("window is never closed", context.state.close_calls == 0);
    })
)

GUNIT_TEST(invalid_mount_rejects_game,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_state = &context.state;
    glint__test_reset_raylib();
    context.alloc = galloc_malloc_allocator();
    const glint_Mount mounts[] = {
        {.point = "/extra", .root = glint__test_extra_root},
        {.point = "/bad", .root = NULL}
    };
    const glint_Config config = {
        .asset_root = glint__test_asset_root,
        .mounts = mounts,
        .mount_count = 2,
        .headless = true
    };

    context.game = glint_new(context.alloc, &config);
    gunit_assert("invalid mount rejects game", context.game == NULL);
)

GUNIT_TEST(additional_mount,
    glint__WindowContext context = {0};

    GUNIT_SETUP({
        gunit_defer(glint__test_context_free, &context);
        const glint_Mount mount = {.point = "/extra", .root = glint__test_extra_root};
        const glint_Config config = {
            .asset_root = glint__test_asset_root,
            .mounts = &mount,
            .mount_count = 1,
            .initial_script = "/game.lua",
            .headless = true
        };
        gunit_assert("create headless game with additional mount", glint__test_context_create(&context, &config));
    })

    GUNIT_SECTION("assets are readable in the mounted directory", {
        size_t size = 0;
        context.source = gvfs_read(context.alloc, glint_vfs(context.game), "/extra/atlas.json", &size);
        gunit_assert("additional mount resolves asset", context.source != NULL && size != 0);
    })

    GUNIT_SECTION("assets are still readable at the root", {
        size_t size = 0;
        context.source = gvfs_read(context.alloc, glint_vfs(context.game), "/game.lua", &size);
        gunit_assert("asset root remains readable", context.source != NULL && size != 0);
    })
)

GUNIT_TEST(raylib_logging,
    glint__WindowContext context = {0};

    GUNIT_SETUP({
        gunit_defer(glint__test_context_free, &context);
        glint__test_state = &context.state;
        glint__test_reset_raylib();
        gunit_assert("open stub window", gplatform_window_open(320, 240, "logging", false, false, 0));
        glog_set_callback(glint__test_capture_log, &context.state);
    })

    GUNIT_SECTION("forwards severity and formatting to the Glint sink", {
        gunit_assert("raylib forwards every severity", context.state.trace_level == LOG_ALL);
        gunit_assert("raylib callback installed", context.state.raylib_log != NULL);
        glog_set_level(GLOG_TRACE);
        TraceLog(LOG_DEBUG, "raylib %s %d%%", "message", 100);
        gunit_assert("raylib severity translated", context.state.log_level == GLOG_DEBUG);
        gunit_assert("raylib formatting preserved", strcmp(context.state.log_message, "raylib message 100%") == 0);
        gunit_assert("one callback per raylib log", context.state.log_count == 1);
    })

    GUNIT_SECTION("uses the configured Glint threshold", {
        glog_set_level(GLOG_ERROR);
        TraceLog(LOG_INFO, "filtered");
        gunit_assert("info is filtered", context.state.log_count == 0);

        TraceLog(LOG_ERROR, "error");
        gunit_assert("one log reaches Glint", context.state.log_count == 1);
        gunit_assert("Raylib error severity is translated", context.state.log_level == GLOG_ERROR);
    })
)

#endif

gunit_main(
    if (gunit_argc != 3) {
        fprintf(stderr, "usage: %s ASSET_DIRECTORY EXTRA_DIRECTORY\n", gunit_argv[0]);
        return EXIT_FAILURE;
    }

    glint__test_asset_root = gunit_argv[1];
    glint__test_extra_root = gunit_argv[2];
);

static void glint__test_reset_raylib(void) {
    memset(glint__test_state, 0, sizeof(*glint__test_state));
    glog_set_callback(NULL, NULL);
    glog_set_level(GLOG_INFO);
}

#ifdef GLINT_TEST_WEB
static bool glint__test_browser_tick(void) {
    if (!gunit_check("loop is scheduled", glint__test_state->scheduled_frame != NULL)) {
        return false;
    }
    glint__test_state->scheduled_frame(glint__test_state->scheduled_arg);

    return true;
}

static bool glint__test_check_loop_allocator(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    const galloc_Allocator alloc = glint_allocator(world);
    return gunit_check("scheduled frame uses the supplied allocator state", alloc.data == user);
}

static void glint__test_end_and_destroy(galloc_Allocator alloc, glint_Game* game, bool ok, void* user) {
    glint__WindowContext* context = user;
    context->completed = ok;
    if (!gunit_check("platform cancels before completion", glint__test_state->scheduled_frame == NULL)) {
        return;
    }
    if (!gunit_check("completion may destroy game", glint_destroy(alloc, game))) {
        return;
    }
    context->game = NULL;
}

static void glint__test_record_completion(galloc_Allocator alloc, glint_Game* game, bool ok, void* user) {
    (void)alloc;
    (void)game;
    (void)ok;
    ((glint__WindowContext*)user)->completed = true;
}

static bool glint__test_fail_frame(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)world;
    (void)user;
    snprintf(error, error_size, "frame failed");
    return false;
}

static void glint__test_failed_and_destroy(galloc_Allocator alloc, glint_Game* game, bool ok, void* user) {
    glint__WindowContext* context = user;
    context->completed = !ok && strcmp(glint_error(game), "frame failed") == 0;
    if (!gunit_check("completion may destroy game", glint_destroy(alloc, game))) {
        return;
    }
    context->game = NULL;
}
#endif

#ifdef GLINT_TEST_PLATFORM
static void glint__test_capture_log(glog_Level level, const char* format, va_list args, void* user) {
    glint__RaylibState* state = user;
    state->log_level = level;
    vsnprintf(state->log_message, sizeof(state->log_message), format, args);
    state->log_count += 1;
}
#endif

static bool glint__test_context_create(glint__WindowContext* context, const glint_Config* config) {
    glint__test_state = &context->state;
    glint__test_reset_raylib();
    if (context->alloc.ftable == NULL) {
        context->alloc = galloc_malloc_allocator();
    }
    context->game = glint_new(context->alloc, config);
    return gunit_check("game created", context->game != NULL);
}

static void glint__test_context_free(void* user) {
    glint__WindowContext* context = user;
    glint__test_state = &context->state;
    glog_set_callback(NULL, NULL);
    if (context->game != NULL) {
        const bool destroyed = glint_destroy(context->alloc, context->game);
        if (gunit_check("cleanup destroys the game", destroyed)) {
            context->game = NULL;
        }
    }
    if (context->state.window_ready) {
        gplatform_window_close();
    }
    galloc_free(context->alloc, context->source);
    context->source = NULL;
    glog_set_level(GLOG_INFO);
    glint__test_state = NULL;
}

#ifndef GLINT_TEST_WEB
static bool glint__test_window_create(glint__WindowContext* context) {
    const glint_Config config = {
        .asset_root = glint__test_asset_root,
        .initial_script = "/game.lua",
        .width = 640,
        .height = 480,
        .title = "windowed",
        .fps = 30,
        .resizable = true,
        .vsync = true,
        .log_level = GLOG_ERROR
    };
    return glint__test_context_create(context, &config);
}
#else
static bool glint__test_web_create(glint__WindowContext* context) {
    const glint_Config config = {.asset_root = glint__test_asset_root};
    return glint__test_context_create(context, &config);
}
#endif

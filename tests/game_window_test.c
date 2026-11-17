#include <glint/glint.h>
#include <glint/unit/unit.h>

#include <raylib.h>

#ifdef GLINT_TEST_WEB
#include <lua.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Raylib stand-ins exercise window lifecycle without a graphics session.

typedef struct glint__RaylibState {
    bool window_ready;
    bool audio_ready;
    uint32_t audio_opens;
    uint32_t audio_closes;
    bool audio_closed_with_window;
    int init_width;
    int init_height;
    char init_title[64];
    unsigned config_flags;
    int target_fps;
    TraceLogCallback raylib_log;
    int trace_level;
    uint32_t polls;
    uint32_t close_calls;
    uint32_t begins;
    uint32_t ends;
    uint32_t clears;
#ifdef GLINT_TEST_PLATFORM
    glog_Level log_level;
    char log_message[128];
    size_t log_count;
#endif
#ifdef GLINT_TEST_WEB
    void (*scheduled_frame)(void*);
    void* scheduled_arg;
    uint32_t cancellations;
#endif
} glint__RaylibState;

typedef struct glint__WindowContext {
    galloc_Allocator alloc;
    glint_Game* game;
    glint__RaylibState state;
    void* source;
    bool completed;
    int allocator_state;
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
#if defined(GLINT_TEST_WEB)
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
    snprintf(glint__test_state->init_title, sizeof(glint__test_state->init_title), "%s", (title != NULL) ? title : "");
    glint__test_state->window_ready = true;
}

bool IsWindowReady(void) {
    return glint__test_state->window_ready;
}

void CloseWindow(void) {
    glint__test_state->window_ready = false;
    glint__test_state->close_calls += 1;
}

void SetConfigFlags(unsigned flags) {
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
GUNIT_TEST(window_configuration,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("window create", glint__test_window_create(&context));
    gunit_assert(
        "window dimensions and title applied",
        context.state.init_width == 640 && context.state.init_height == 480
            && strcmp(context.state.init_title, "windowed") == 0
    );
    gunit_assert(
        "window flags applied",
        (context.state.config_flags & FLAG_VSYNC_HINT) != 0 && (context.state.config_flags & FLAG_WINDOW_RESIZABLE) != 0
    );
    gunit_assert("frame rate and log level applied", context.state.target_fps == 30 && glog_get_level() == GLOG_ERROR);
)

GUNIT_TEST(window_audio_service,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("window create", glint__test_window_create(&context));
    gecs_World* world = glint_world(context.game);
    gaudio_Service* audio = gecs_context_get(world, GECS_CONTEXT(world, gaudio_Service));
    gunit_assert(
        "module provides owned device",
        audio != NULL && audio->module_owned && gaudio_Device_ready(&audio->device)
    );
    gunit_assert("audio device opened once", context.state.audio_opens == 1);
)

GUNIT_TEST(window_frame_renders_once,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("window create", glint__test_window_create(&context));
    gunit_assert("run windowed frame", glint_frame(context.alloc, context.game, 0.016f));
    gunit_assert(
        "begin clear and end drawing",
        context.state.begins == 1 && context.state.ends == 1 && context.state.clears == 1
    );
)

GUNIT_TEST(native_loop_renders_until_close,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("window create", glint__test_window_create(&context));
    gunit_assert("run native loop", glint_run(context.alloc, context.game));
    gunit_assert(
        "loop renders until close",
        context.state.begins == 3 && context.state.ends == 3 && context.state.polls == 4
    );
)

GUNIT_TEST(window_shutdown_closes_audio_first,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("window create", glint__test_window_create(&context));
    gunit_assert("game destruction succeeds", glint_destroy(context.alloc, context.game));
    context.game = NULL;
    gunit_assert(
        "audio device closes before window",
        context.state.audio_closes == 1 && context.state.audio_closed_with_window
    );
    gunit_assert("window closes once", context.state.close_calls == 1 && !context.state.window_ready);
)

#else
GUNIT_TEST(web_loop_normal_completion,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("web create", glint__test_web_create(&context));
    gunit_assert(
        "schedule game",
        glint_run_with_completion(context.alloc, context.game, glint__test_end_and_destroy, &context)
    );
    for (size_t i = 0; i < 4; i++) {
        gunit_assert("browser tick", glint__test_browser_tick());
    }
    gunit_assert(
        "completion destroys game",
        context.completed && context.game == NULL && context.state.close_calls == 1
    );
)

GUNIT_TEST(web_destroy_cancels_without_completion,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("web create", glint__test_web_create(&context));
    gunit_assert(
        "schedule game with completion",
        glint_run_with_completion(context.alloc, context.game, glint__test_record_completion, &context)
    );
    gunit_assert("loop awaits browser frames", context.state.scheduled_frame != NULL && !context.completed);
    gunit_assert("browser tick", glint__test_browser_tick());
    gunit_assert("loop remains active before destruction", context.state.scheduled_frame != NULL && !context.completed);
    gunit_assert("destroy active game", glint_destroy(context.alloc, context.game));
    context.game = NULL;
    gunit_assert("destroy cancels loop", context.state.scheduled_frame == NULL && context.state.cancellations == 1);
    gunit_assert("destroy never invokes completion", !context.completed);
)

GUNIT_TEST(web_loop_failure_completion,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gmod_Module failure = {GMOD_INIT, .name = "failure", .update = glint__test_fail_frame};
    glint_Config config = {.asset_root = glint__test_asset_root, .modules = &failure, .module_count = 1};
    gunit_assert("context create", glint__test_context_create(&context, &config));
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

GUNIT_TEST(web_loop_schedules_without_blocking,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("web create", glint__test_web_create(&context));
    gunit_assert(
        "schedule without blocking",
        glint_run(context.alloc, context.game) && context.state.scheduled_frame != NULL && context.state.begins == 0
    );
    gunit_assert("duplicate scheduling rejected", !glint_run(context.alloc, context.game));
    gunit_assert("browser tick", glint__test_browser_tick());
    gunit_assert("scheduled frame draws", context.state.begins == 1 && context.state.ends == 1);
)

GUNIT_TEST(web_loop_preserves_allocator_state,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    context.alloc = galloc_malloc_allocator();
    context.alloc.data = &context.allocator_state;
    gmod_Module module =
        {GMOD_INIT, .name = "allocator", .user = &context.allocator_state, .update = glint__test_check_loop_allocator};
    glint_Config config = {.asset_root = glint__test_asset_root, .modules = &module, .module_count = 1};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gunit_assert("schedule allocator game", glint_run(context.alloc, context.game));
    gunit_assert("browser tick", glint__test_browser_tick());
    gunit_assert("allocator check ran in scheduled frame", context.state.begins == 1 && context.state.ends == 1);
)

GUNIT_TEST(web_window_close_cancels_without_destroying,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("web create", glint__test_web_create(&context));
    gunit_assert("schedule game", glint_run(context.alloc, context.game));
    for (size_t i = 0; i < 4; i++) {
        gunit_assert("browser tick", glint__test_browser_tick());
    }
    gunit_assert(
        "window close cancels loop",
        context.state.scheduled_frame == NULL && context.state.cancellations == 1
    );
    gunit_assert("window close does not destroy game", context.state.close_calls == 0);
)

GUNIT_TEST(web_loop_restart_can_be_destroyed,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    gunit_assert("web create", glint__test_web_create(&context));
    gunit_assert("schedule game", glint_run(context.alloc, context.game));
    for (size_t i = 0; i < 4; i++) {
        gunit_assert("browser tick", glint__test_browser_tick());
    }
    gunit_assert("restart after close", glint_run(context.alloc, context.game));
    glint__test_context_free(&context);
    gunit_assert(
        "destroy cancels before closing",
        context.state.scheduled_frame == NULL && context.state.cancellations == 2 && context.state.close_calls == 1
    );
)

GUNIT_TEST(web_script_error_keeps_loop_running,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Config config = {.asset_root = glint__test_asset_root, .initial_script = "/game.lua"};
    gunit_assert("context create", glint__test_context_create(&context, &config));
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
GUNIT_TEST(headless_game_skips_window_configuration,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Config config =
        {.asset_root = glint__test_asset_root, .initial_script = "/game.lua", .log_level = GLOG_WARN, .headless = true};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gunit_assert(
        "headless skips window configuration",
        context.state.init_width == 0 && context.state.config_flags == 0 && context.state.target_fps == 0
    );
    gunit_assert("headless log level applied", glog_get_level() == GLOG_WARN);
)

GUNIT_TEST(headless_game_does_not_draw_or_run_loop,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Config config = {.asset_root = glint__test_asset_root, .initial_script = "/game.lua", .headless = true};
    gunit_assert("context create", glint__test_context_create(&context, &config));
    gunit_assert("headless frame succeeds", glint_frame(context.alloc, context.game, 0.016f));
    gunit_assert("headless does not draw", context.state.begins == 0);
    gunit_assert("headless cannot run window loop", !glint_run(context.alloc, context.game));
    glint__test_context_free(&context);
    gunit_assert("headless shutdown leaves window untouched", context.state.close_calls == 0);
)

GUNIT_TEST(invalid_mount_rejects_game,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_state = &context.state;
    glint__test_reset_raylib();
    context.alloc = galloc_malloc_allocator();
    glint_Mount mounts[] = {{.point = "/extra", .root = glint__test_extra_root}, {.point = "/bad", .root = NULL}};
    glint_Config config = {.asset_root = glint__test_asset_root, .mounts = mounts, .mount_count = 2, .headless = true};
    context.game = glint_new(context.alloc, &config);
    gunit_assert("invalid mount rejects game", context.game == NULL);
)

GUNIT_TEST(additional_mount_resolves_assets,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Mount mount = {.point = "/extra", .root = glint__test_extra_root};
    glint_Config config = {
        .asset_root = glint__test_asset_root,
        .mounts = &mount,
        .mount_count = 1,
        .initial_script = "/game.lua",
        .headless = true
    };
    gunit_assert("context create", glint__test_context_create(&context, &config));
    size_t size = 0;
    context.source = gvfs_read(context.alloc, glint_vfs(context.game), "/extra/atlas.json", &size);
    gunit_assert("additional mount resolves asset", context.source != NULL && size != 0);
)

GUNIT_TEST(additional_mount_preserves_asset_root,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint_Mount mount = {.point = "/extra", .root = glint__test_extra_root};
    glint_Config config = {
        .asset_root = glint__test_asset_root,
        .mounts = &mount,
        .mount_count = 1,
        .initial_script = "/game.lua",
        .headless = true
    };
    gunit_assert("context create", glint__test_context_create(&context, &config));
    size_t size = 0;
    context.source = gvfs_read(context.alloc, glint_vfs(context.game), "/game.lua", &size);
    gunit_assert("asset root remains readable", context.source != NULL && size != 0);
)

GUNIT_TEST(raylib_logs_use_glint_sink,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_state = &context.state;
    glint__test_reset_raylib();
    gunit_assert("open stub window", gplatform_window_open(320, 240, "logging", false, false, 0));
    gunit_assert("raylib forwards every severity", context.state.trace_level == LOG_ALL);
    gunit_assert("raylib callback installed", context.state.raylib_log != NULL);
    glog_set_callback(glint__test_capture_log, NULL);
    glog_set_level(GLOG_TRACE);
    TraceLog(LOG_DEBUG, "raylib %s %d%%", "message", 100);
    gunit_assert("raylib severity translated", context.state.log_level == GLOG_DEBUG);
    gunit_assert("raylib formatting preserved", strcmp(context.state.log_message, "raylib message 100%") == 0);
    gunit_assert("one callback per raylib log", context.state.log_count == 1);
)

GUNIT_TEST(raylib_logs_follow_glint_filter,
    glint__WindowContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_state = &context.state;
    glint__test_reset_raylib();
    gunit_assert("open stub window", gplatform_window_open(320, 240, "logging", false, false, 0));
    glog_set_callback(glint__test_capture_log, NULL);
    glog_set_level(GLOG_ERROR);
    TraceLog(LOG_INFO, "filtered");
    gunit_assert("Glint controls raylib filtering", context.state.log_count == 0);
    TraceLog(LOG_ERROR, "error");
    gunit_assert("raylib errors reach Glint", context.state.log_count == 1 && context.state.log_level == GLOG_ERROR);
)

#endif

gunit_main(
    if (gunit_argc != 3) {
        fprintf(stderr, "usage: %s ASSET_DIRECTORY EXTRA_DIRECTORY\n", gunit_argv[0]);
        return EXIT_FAILURE;
    } glint__test_asset_root = gunit_argv[1];
    glint__test_extra_root = gunit_argv[2]
);

static void glint__test_reset_raylib(void) {
    memset(glint__test_state, 0, sizeof(*glint__test_state));
    glog_set_callback(NULL, NULL);
    glog_set_level(GLOG_INFO);
}

#if defined(GLINT_TEST_WEB)
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
    galloc_Allocator alloc = glint_allocator(world);
    (void)world;
    if (!gunit_check("scheduled frame uses the supplied allocator state", alloc.data == user)) {
        return false;
    }
    return true;
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
    (void)user;
    glint__test_state->log_level = level;
    vsnprintf(glint__test_state->log_message, sizeof(glint__test_state->log_message), format, args);
    glint__test_state->log_count += 1;
}
#endif

static bool glint__test_context_create(glint__WindowContext* context, const glint_Config* config) {
    glint__test_state = &context->state;
    glint__test_reset_raylib();
    if (context->alloc.ftable == NULL) {
        context->alloc = galloc_malloc_allocator();
    }
    context->game = glint_new(context->alloc, config);
    if (!gunit_check("game created", context->game != NULL)) {
        return false;
    }

    return true;
}

static void glint__test_context_free(void* user) {
    glint__WindowContext* context = user;
    glint__test_state = &context->state;
    glog_set_callback(NULL, NULL);
    if (context->game != NULL) {
        glint_destroy(context->alloc, context->game);
        context->game = NULL;
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
    glint_Config config = {
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
    if (!glint__test_context_create(context, &config)) {
        return false;
    }

    return true;
}
#else
static bool glint__test_web_create(glint__WindowContext* context) {
    glint_Config config = {.asset_root = glint__test_asset_root};
    if (!glint__test_context_create(context, &config)) {
        return false;
    }

    return true;
}
#endif

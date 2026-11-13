#include <glint/game.h>

#include <mewunit.h>
#include <raylib.h>
#ifdef GGAME_TEST_WEB
#include <lua.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Full raylib stand-ins let the windowed game lifecycle run in CI without a
 * graphics session. Every raylib symbol the engine references is defined here
 * so no raylib object is linked into this test. */

static bool ggame__test_window_ready;
static int ggame__test_init_width;
static int ggame__test_init_height;
static char ggame__test_init_title[64];
static unsigned ggame__test_config_flags;
static int ggame__test_target_fps;
static int ggame__test_trace_level;
static unsigned ggame__test_polls;
static unsigned ggame__test_close_calls;
static unsigned ggame__test_begins;
static unsigned ggame__test_ends;
static unsigned ggame__test_clears;

#ifdef GGAME_TEST_WEB
static void (*ggame__test_scheduled_frame)(void *);
static void *ggame__test_scheduled_arg;
static unsigned ggame__test_cancellations;

void emscripten_set_main_loop_arg(void (*callback)(void *), void *arg, int fps, int simulate_infinite_loop) {
    mewrequire("browser-driven asynchronous loop", fps == 0 && !simulate_infinite_loop && !ggame__test_scheduled_frame);
    ggame__test_scheduled_frame = callback;
    ggame__test_scheduled_arg = arg;
}

void emscripten_cancel_main_loop(void) {
    mewrequire("cancel only a registered loop", ggame__test_scheduled_frame != NULL);
    ggame__test_scheduled_frame = NULL;
    ggame__test_scheduled_arg = NULL;
    ggame__test_cancellations++;
}

static void ggame__test_browser_tick(void) {
    mewrequire("loop is scheduled", ggame__test_scheduled_frame != NULL);
    ggame__test_scheduled_frame(ggame__test_scheduled_arg);
}

static void ggame__test_check_loop_allocator(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)world;
    mewrequire("scheduled frame uses the supplied allocator state", alloc.data == user);
}
#endif

void InitWindow(int width, int height, const char *title) {
    ggame__test_init_width = width;
    ggame__test_init_height = height;
    snprintf(ggame__test_init_title, sizeof(ggame__test_init_title), "%s", title ? title : "");
    ggame__test_window_ready = true;
}

bool IsWindowReady(void) {
    return ggame__test_window_ready;
}

void CloseWindow(void) {
    ggame__test_window_ready = false;
    ggame__test_close_calls++;
}

void SetConfigFlags(unsigned flags) {
    ggame__test_config_flags |= flags;
}

void SetTargetFPS(int fps) {
    ggame__test_target_fps = fps;
}

void SetTraceLogLevel(int level) {
    ggame__test_trace_level = level;
}

bool WindowShouldClose(void) {
    return ++ggame__test_polls > 3;
}

float GetFrameTime(void) {
    return 1.0f / 60.0f;
}

double GetTime(void) {
    return 0.0;
}

int GetScreenWidth(void) {
    return ggame__test_init_width;
}

int GetScreenHeight(void) {
    return ggame__test_init_height;
}

void BeginDrawing(void) {
    ggame__test_begins++;
}

void EndDrawing(void) {
    ggame__test_ends++;
}

void ClearBackground(Color color) {
    (void)color;
    ggame__test_clears++;
}

void TraceLog(int level, const char *format, ...) {
    (void)level;
    (void)format;
}

bool IsAudioDeviceReady(void) {
    return false;
}

void SetAudioStreamBufferSizeDefault(int size) {
    (void)size;
}

void InitAudioDevice(void) {}

void CloseAudioDevice(void) {}

Image LoadImageFromMemory(const char *type, const unsigned char *bytes, int size) {
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

static const char *ggame__test_asset_root;
static const char *ggame__test_extra_root;

static void ggame__test_reset_raylib(void) {
    ggame__test_window_ready = false;
    ggame__test_init_width = 0;
    ggame__test_init_height = 0;
    ggame__test_init_title[0] = '\0';
    ggame__test_config_flags = 0;
    ggame__test_target_fps = 0;
    ggame__test_trace_level = 0;
    ggame__test_polls = 0;
    ggame__test_close_calls = 0;
    ggame__test_begins = 0;
    ggame__test_ends = 0;
    ggame__test_clears = 0;
}

#ifndef GGAME_TEST_WEB
TEST(window_config_and_lifecycle, {
    ggame__test_reset_raylib();
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame_Config config = {
        .asset_root = ggame__test_asset_root,
        .initial_script = "/game.lua",
        .width = 640,
        .height = 480,
        .title = "windowed",
        .fps = 30,
        .resizable = true,
        .vsync = true,
        .log_level = GLOG_ERROR,
    };
    ggame_Game *game = ggame_new(alloc, &config);
    mewassert("Game expectation failed", game);
    mewassert("Game expectation failed", ggame__test_init_width == 640 && ggame__test_init_height == 480 && !strcmp(ggame__test_init_title, "windowed"));
    mewassert("Game expectation failed", (ggame__test_config_flags & FLAG_VSYNC_HINT) && (ggame__test_config_flags & FLAG_WINDOW_RESIZABLE));
    mewassert("Game expectation failed", ggame__test_target_fps == 30 && ggame__test_trace_level == GLOG_ERROR);
    mewassert("Game expectation failed", ggame_frame(alloc, game, 0.016f));
    mewassert("Game expectation failed", ggame__test_begins == 1 && ggame__test_ends == 1 && ggame__test_clears == 1);
    mewassert("Game expectation failed", ggame_run(alloc, game));
    mewassert("Game expectation failed", ggame__test_begins == 4 && ggame__test_ends == 4 && ggame__test_polls == 4);
    ggame_destroy(alloc, game);
    mewassert("Game expectation failed", ggame__test_close_calls == 1 && !ggame__test_window_ready);
})
#else
TEST(web_loop_lifecycle, {
    ggame__test_reset_raylib();
    ggame__test_cancellations = 0;
    galloc_Allocator alloc = galloc_malloc_allocator();
    int allocator_state;
    alloc.data = &allocator_state;
    gmod_Module module = {GMOD_INIT, .name = "allocator", .user = &allocator_state, .update = ggame__test_check_loop_allocator};
    ggame_Config config = {
        .asset_root = ggame__test_asset_root, .initial_script = "/game.lua", .modules = &module, .module_count = 1
    };
    ggame_Game *game = ggame_new(alloc, &config);
    mewassert("create game", game);
    mewassert("schedule without blocking", ggame_run(alloc, game) && ggame__test_scheduled_frame && ggame__test_begins == 0);
    mewassert("reject duplicate scheduling", !ggame_run(alloc, game));
    ggame__test_browser_tick();
    mewassert("frame draws", ggame__test_begins == 1 && ggame__test_ends == 1);
    ggame__test_browser_tick();
    ggame__test_browser_tick();
    ggame__test_browser_tick();
    mewassert("window close cancels without destruction", !ggame__test_scheduled_frame && ggame__test_cancellations == 1 && ggame__test_close_calls == 0);
    mewassert("restart after close", ggame_run(alloc, game));
    ggame_destroy(alloc, game);
    mewassert("destroy cancels before closing", !ggame__test_scheduled_frame && ggame__test_cancellations == 2 && ggame__test_close_calls == 1);
})

TEST(web_loop_frame_error, {
    ggame__test_reset_raylib();
    ggame__test_cancellations = 0;
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame_Config config = {.asset_root = ggame__test_asset_root, .initial_script = "/game.lua"};
    ggame_Game *game = ggame_new(alloc, &config);
    mewassert("create game", game);
    lua_State *lua = gscript_state(ggame_script(game));
    lua_pushnil(lua);
    lua_setglobal(lua, "Timer");
    mewassert("schedule", ggame_run(alloc, game));
    ggame__test_browser_tick();
    mewassert("error cancels and remains inspectable", !ggame__test_scheduled_frame && ggame__test_cancellations == 1
        && strstr(ggame_error(game), "Timer") && ggame__test_begins == 0);
    ggame_destroy(alloc, game);
    mewassert("already cancelled loop not cancelled twice", ggame__test_cancellations == 1 && ggame__test_close_calls == 1);
})
#endif

TEST(headless_game_skips_window, {
    ggame__test_reset_raylib();
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame_Config config = {
        .asset_root = ggame__test_asset_root,
        .initial_script = "/game.lua",
        .log_level = GLOG_WARNING,
        .headless = true,
    };
    ggame_Game *game = ggame_new(alloc, &config);
    mewassert("Game expectation failed", game);
    mewassert("Game expectation failed", ggame__test_init_width == 0 && ggame__test_config_flags == 0 && ggame__test_target_fps == 0);
    mewassert("Game expectation failed", ggame__test_trace_level == GLOG_WARNING);
    mewassert("Game expectation failed", ggame_frame(alloc, game, 0.016f));
    mewassert("Game expectation failed", ggame__test_begins == 0 && !ggame_run(alloc, game));
    ggame_destroy(alloc, game);
    mewassert("Game expectation failed", ggame__test_close_calls == 0);
})

TEST(additional_mounts_resolve, {
    ggame__test_reset_raylib();
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame_Mount mounts[] = {
        {.point = "/extra", .root = ggame__test_extra_root},
        {.point = "/bad", .root = NULL},
    };
    ggame_Config config = {.asset_root = ggame__test_asset_root, .mounts = mounts, .mount_count = 2, .headless = true};
    mewassert("Game expectation failed", !ggame_new(alloc, &config));
    ggame_Game *game = ggame_new(
        alloc,
        &(ggame_Config) {
            .asset_root = ggame__test_asset_root,
            .mounts = mounts,
            .mount_count = 1,
            .initial_script = "/game.lua",
            .headless = true
        }
    );
    mewassert("Game expectation failed", game);
    gvfs_Vfs *vfs = ggame_vfs(game);
    size_t size = 0;
    char *source = gvfs_read(alloc, vfs, "/extra/atlas.json", &size);
    mewassert("Game expectation failed", source && size);
    galloc_free(alloc, source);
    source = gvfs_read(alloc, vfs, "/game.lua", &size);
    mewassert("Game expectation failed", source && size);
    galloc_free(alloc, source);
    ggame_destroy(alloc, game);
})

mewtest_main(if (mew_test_argc != 3) {
    fprintf(stderr, "usage: %s ASSET_DIRECTORY EXTRA_DIRECTORY\n", mew_test_argv[0]);
    return EXIT_FAILURE;
} ggame__test_asset_root = mew_test_argv[1]; ggame__test_extra_root = mew_test_argv[2]);

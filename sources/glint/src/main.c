#include <glint/game.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <raylib.h>

/* The browser owns the scheduled callback after main returns. */
static galloc_Allocator ggame__entry_alloc;
static ggame_Game *ggame__entry_game;

static void ggame__main_entry_frame(void *unused) {
    (void)unused;
    bool close = WindowShouldClose();
    if (!close && !ggame_frame(ggame__entry_alloc, ggame__entry_game, GetFrameTime())) {
        fprintf(stderr, "glint: %s\n", ggame_error(ggame__entry_game));
        close = true;
    }
    if (close) {
        emscripten_cancel_main_loop();
        ggame_destroy(ggame__entry_alloc, ggame__entry_game);
        ggame__entry_game = NULL;
    }
}
#endif

int main(int argc, char **argv) {
    galloc_Allocator alloc = galloc_malloc_allocator();
    ggame_Config config = ggame_configure(argc, argv);
    if (config.headless) {
        fprintf(stderr, "glint: the default entry point requires a graphical game\n");
        return 1;
    }
    ggame_Game *game = ggame_new(alloc, &config);
    if (!game) return 1;
#ifdef __EMSCRIPTEN__
    ggame__entry_alloc = alloc;
    ggame__entry_game = game;
    emscripten_set_main_loop_arg(ggame__main_entry_frame, NULL, 0, false);
    return 0;
#else
    bool ok = ggame_run(alloc, game);
    if (!ok) fprintf(stderr, "glint: %s\n", ggame_error(game));
    ggame_destroy(alloc, game);
    return ok ? 0 : 1;
#endif
}

/* Link this driver against either actual example entry point, compiled with
 * -Dmain=example_entry. No real window, browser or graphics backend needed. */
#include <glint/game.h>
#include <assert.h>
#include <string.h>

int example_entry(int argc, char **argv);

struct ggame_Game { int alive; };
static struct ggame_Game ggame__test_game;
static int ggame__test_allocator_state;
static int ggame__test_destroys, ggame__test_frames;
static bool ggame__test_fail_create, ggame__test_fail_frame;

galloc_Allocator galloc_malloc_allocator(void) { return (galloc_Allocator){.data = &ggame__test_allocator_state}; }
ggame_Game *ggame_new(galloc_Allocator alloc, const ggame_Config *config) {
    assert(alloc.data == &ggame__test_allocator_state);
    assert(!strcmp(config->asset_root, "test-assets"));
    assert(!strcmp(config->initial_script, "/game.lua"));
    if (ggame__test_fail_create) return NULL;
    ggame__test_game.alive = 1;
    return &ggame__test_game;
}
void ggame_destroy(galloc_Allocator alloc, ggame_Game *value) {
    assert(alloc.data == &ggame__test_allocator_state && value == &ggame__test_game && ggame__test_game.alive);
    ggame__test_game.alive = 0;
    ggame__test_destroys++;
}
bool ggame_frame(galloc_Allocator alloc, ggame_Game *value, float dt) {
    assert(alloc.data == &ggame__test_allocator_state && value == &ggame__test_game && ggame__test_game.alive && dt > 0);
    ggame__test_frames++;
    return !ggame__test_fail_frame;
}
const char *ggame_error(const ggame_Game *value) {
    assert(value == &ggame__test_game && ggame__test_game.alive);
    return "test frame error";
}
bool ggame_run(galloc_Allocator alloc, ggame_Game *value) {
    return ggame_frame(alloc, value, 1.0f);
}

#ifdef __EMSCRIPTEN__
static void (*ggame__test_callback)(void *);
static void *ggame__test_callback_arg;
static int ggame__test_cancellations;
static bool ggame__test_should_close;
void emscripten_set_main_loop_arg(void (*fn)(void *), void *arg, int fps, int infinite) {
    assert(!ggame__test_callback && fps == 0 && !infinite);
    ggame__test_callback = fn;
    ggame__test_callback_arg = arg;
}
void emscripten_cancel_main_loop(void) {
    assert(ggame__test_callback && ggame__test_game.alive);
    ggame__test_callback = NULL;
    ggame__test_cancellations++;
}
bool WindowShouldClose(void) { return ggame__test_should_close; }
float GetFrameTime(void) { return 1.0f / 60.0f; }
static void ggame__test_tick(void) { assert(ggame__test_callback); ggame__test_callback(ggame__test_callback_arg); }
#endif

int main(void) {
    ggame__test_fail_create = true;
    assert(example_entry(0, NULL) == 1 && ggame__test_destroys == 0);
    ggame__test_fail_create = false;
    assert(example_entry(0, NULL) == 0);
#ifdef __EMSCRIPTEN__
    assert(ggame__test_callback && ggame__test_game.alive && ggame__test_destroys == 0 && ggame__test_frames == 0);
    ggame__test_tick();
    ggame__test_tick();
    assert(ggame__test_frames == 2 && ggame__test_game.alive && ggame__test_destroys == 0);
    ggame__test_should_close = true;
    ggame__test_tick();
    assert(!ggame__test_callback && !ggame__test_game.alive && ggame__test_destroys == 1 && ggame__test_cancellations == 1 && ggame__test_frames == 2);
    ggame__test_should_close = false;
    ggame__test_fail_frame = true;
    assert(example_entry(0, NULL) == 0 && ggame__test_callback && ggame__test_game.alive);
    ggame__test_tick();
    assert(!ggame__test_callback && !ggame__test_game.alive && ggame__test_destroys == 2 && ggame__test_cancellations == 2 && ggame__test_frames == 3);
#else
    assert(!ggame__test_game.alive && ggame__test_destroys == 1 && ggame__test_frames == 1);
    ggame__test_fail_frame = true;
    assert(example_entry(0, NULL) == 1 && ggame__test_destroys == 2 && ggame__test_frames == 2);
#endif
    return 0;
}

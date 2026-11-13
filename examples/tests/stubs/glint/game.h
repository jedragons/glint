#ifndef EXAMPLE_TEST_GAME_H
#define EXAMPLE_TEST_GAME_H
#include <stdbool.h>

typedef struct { void *data; } galloc_Allocator;
typedef struct ggame_Game ggame_Game;
typedef struct {
    const char *asset_root;
    const char *initial_script;
    int width, height;
    const char *title;
    bool headless;
} ggame_Config;
ggame_Config ggame_configure(int argc, char **argv);

galloc_Allocator galloc_malloc_allocator(void);
ggame_Game *ggame_new(galloc_Allocator, const ggame_Config *);
void ggame_destroy(galloc_Allocator, ggame_Game *);
bool ggame_run(galloc_Allocator, ggame_Game *);
bool ggame_frame(galloc_Allocator, ggame_Game *, float);
const char *ggame_error(const ggame_Game *);
#endif

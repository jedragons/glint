#include <glint/game.h>
#include <glint/render.h>
#include <glint/script_entity.h>

int main(int argc, char **argv) {
    (void)argv;
    /* Exercise the full static link without opening a window when run normally. */
    if (argc > 1) {
        galloc_Allocator alloc = galloc_malloc_allocator();
        ggame_Config config = {0};
        ggame_Game *game = ggame_new(alloc, &config);
        ggame_destroy(alloc, game);
    }
    return 0;
}

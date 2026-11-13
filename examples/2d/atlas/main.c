#include <glint/game.h>

ggame_Config ggame_configure(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return (ggame_Config){
        .asset_root = ASSETS_DIR,
        .initial_script = "/game.lua",
        .width = 800,
        .height = 600,
        .title = "glint",
    };
}

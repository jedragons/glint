#include <glint/glint.h>

glint_Config glint_main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    return (glint_Config) {
        .asset_root = ASSETS_DIR,
        .initial_script = "/game.lua",
        .width = 800,
        .height = 600,
        .title = "2D Atlas",
    };
}

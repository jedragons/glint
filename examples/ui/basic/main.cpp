#include <glint/glint.hpp>

auto main() -> int {
    auto config = glint::default_config("ui-basic")
                      .add_native_mount_point("/", ASSETS_DIR)
                      .set_initial_behavior("/game.lua");
    auto game = glint::Game {config};
    return game.run();
}

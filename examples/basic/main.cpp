#include <glint/glint.hpp>

auto glint_main() -> glint::Config {
    return glint::default_config().add_native_mount_point("/", ASSETS_DIR).set_initial_behavior("/game.lua");
}

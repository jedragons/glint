#include <raylib.h>

#include <glint/dim2.hpp>
#include <glint/dim2/physics/transform.hpp>
#include <glint/dim2/rendering/ecs/animated_sprite.hpp>
#include <glint/dim2/rendering/ecs/sprite.hpp>
#include <glint/glint.hpp>

auto main() -> int {
    auto config =
        glint::default_config("atlas").add_native_mount_point("/", ASSETS_DIR).set_initial_behavior("/game.lua");
    auto game = glint::Game {config};

    auto& registry = game.registry();

    auto atlas = glint::dim2::load_atlas(registry, "/atlas.json");

    auto sprite = registry.create();
    registry.emplace<glint::dim2::Transform2D>(sprite, Vector2 {96.0f, 64.0f});
    registry.emplace<glint::dim2::SpriteComp>(sprite, atlas->sprite("frame0"));

    auto animated = registry.create();
    registry.emplace<glint::dim2::Transform2D>(animated, Vector2 {224.0f, 64.0f});
    registry.emplace<glint::dim2::AnimatedSpriteComp>(animated, atlas->animation("walk"));

    return game.run();
}

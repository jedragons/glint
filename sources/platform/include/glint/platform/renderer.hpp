#pragma once

#include <memory>

#include <glint/math.hpp>
#include <glint/platform/texture.hpp>

namespace glint::platform {

struct TextureDrawParams {
    Rect source = {};
    Rect dest = {};
    Vec2 origin = {};
    float rotation = {};
    Color tint = White;
};

class Renderer {
  public:
    static auto create() -> Renderer;

    auto clear_background(Color color) -> void;

    auto draw_texture(const Texture& texture, Vec2 position, Color tint = White) -> void;

    auto draw_texture(const Texture& texture, TextureDrawParams params = {}) -> void;

  private:
    struct Data {
        std::unique_ptr<void*> handle;
    } m;

    Renderer() = default;
};

} // namespace glint::platform

namespace glint {

using platform::Renderer;

}

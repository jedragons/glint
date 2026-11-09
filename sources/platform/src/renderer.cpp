#include <glint/platform/renderer.hpp>

#include <raylib.h>

namespace glint::platform {

auto Renderer::create() -> Renderer {
    return {};
}

auto Renderer::clear_background(Color color) -> void {
    auto raylib_color = ::Color {
        .r = static_cast<unsigned char>(color.r),
        .g = static_cast<unsigned char>(color.g),
        .b = static_cast<unsigned char>(color.b),
        .a = static_cast<unsigned char>(color.a),
    };
    ::ClearBackground(raylib_color);
}

auto Renderer::draw_texture(const Texture& texture, Vec2 position, Color tint) -> void {
    draw_texture(
        texture,
        TextureDrawParams {
            .source =
                Rect {
                    .x = 0,
                    .y = 0,
                    .w = static_cast<float>(texture.width()),
                    .h = static_cast<float>(texture.height()),
                },
            .dest =
                Rect {
                    .x = position.x,
                    .y = position.y,
                    .w = static_cast<float>(texture.width()),
                    .h = static_cast<float>(texture.height()),
                },
            .origin = Vec2 {},
            .rotation = 0,
            .tint = tint,
        }
    );
}

auto Renderer::draw_texture(const Texture& texture, TextureDrawParams params) -> void {
    auto raylib_texture = ::Texture2D {
        .id = static_cast<unsigned int>(texture.id()),
        .width = static_cast<int>(texture.width()),
        .height = static_cast<int>(texture.width()),
        .mipmaps = static_cast<int>(texture.mipmaps()),
        .format = static_cast<int>(texture.format()),
    };
    auto source = ::Rectangle(params.source.x, params.source.y, params.source.w, params.source.h);
    auto dest = ::Rectangle(params.dest.x, params.dest.y, params.dest.w, params.dest.h);
    auto origin = ::Vector2(params.origin.x, params.origin.y);
    auto tint = ::Color {
        .r = static_cast<unsigned char>(params.tint.r),
        .g = static_cast<unsigned char>(params.tint.g),
        .b = static_cast<unsigned char>(params.tint.b),
        .a = static_cast<unsigned char>(params.tint.a),
    };
    ::DrawTexturePro(raylib_texture, source, dest, origin, params.rotation, tint);
}

} // namespace glint::platform

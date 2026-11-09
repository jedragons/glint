#include <glint/platform/texture.hpp>

#include <raylib.h>

namespace glint::platform {

static auto texture_to_raylib(const Texture& t) -> ::Texture2D {
    return {
        .id = static_cast<unsigned int>(t.id()),
        .width = static_cast<int>(t.width()),
        .height = static_cast<int>(t.height()),
        .mipmaps = static_cast<int>(t.mipmaps()),
        .format = static_cast<int>(t.format()),
    };
}

auto Texture::load(const std::string& filetype, std::span<char> data) noexcept -> Texture {
    auto image = ::LoadImageFromMemory(
        filetype.c_str(),
        // NOLINTNEXTLINE: Cast from char* to unsigned char* for raylib is safe
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size())
    );
    auto texture = ::LoadTextureFromImage(image);
    ::UnloadImage(image);

    return Texture {Data {
        .id = static_cast<uint32_t>(texture.id),
        .width = static_cast<int32_t>(texture.width),
        .height = static_cast<int32_t>(texture.height),
        .mipmaps = static_cast<int32_t>(texture.mipmaps),
        .format = static_cast<int32_t>(texture.format),
    }};
}

Texture::Texture(Texture&& other) noexcept : m(other.m) {
    other.m = {};
}

auto Texture::operator=(Texture&& other) noexcept -> Texture& {
    std::swap(m, other.m);
    return *this;
}

Texture::~Texture() noexcept {
    if (m.id == 0) return;
    ::UnloadTexture(texture_to_raylib(*this));
}

auto Texture::id() const noexcept -> uint32_t {
    return m.id;
}

auto Texture::width() const noexcept -> int32_t {
    return m.width;
}

auto Texture::height() const noexcept -> int32_t {
    return m.height;
}

auto Texture::mipmaps() const noexcept -> int32_t {
    return m.mipmaps;
}

auto Texture::format() const noexcept -> int32_t {
    return m.format;
}

auto Texture::valid() const noexcept -> bool {
    return IsTextureValid(texture_to_raylib(*this));
}

Texture::Texture(Data data) noexcept : m {data} {}

} // namespace glint::platform

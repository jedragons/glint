#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace glint::platform {

class Texture {
  public:
    static auto load(const std::string& filetype, std::span<char> data) noexcept -> Texture;

    Texture() noexcept = default;
    Texture(Texture&& other) noexcept;
    auto operator=(Texture&& other) noexcept -> Texture&;
    ~Texture() noexcept;

    Texture(const Texture&) = delete;
    auto operator=(const Texture&) -> Texture& = delete;

    [[nodiscard]]
    auto id() const noexcept -> uint32_t;

    [[nodiscard]]
    auto width() const noexcept -> int32_t;

    [[nodiscard]]
    auto height() const noexcept -> int32_t;

    [[nodiscard]]
    auto mipmaps() const noexcept -> int32_t;

    [[nodiscard]]
    auto format() const noexcept -> int32_t;

    [[nodiscard]]
    auto valid() const noexcept -> bool;

  private:
    struct Data {
        uint32_t id;
        int32_t width;
        int32_t height;
        int32_t mipmaps;
        int32_t format;
    } m;

    explicit Texture(Data data) noexcept;
};

} // namespace glint::platform

namespace glint {

using platform::Texture;

} // namespace glint

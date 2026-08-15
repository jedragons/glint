#pragma once

#include <entt/fwd.hpp>
#include <raylib-cpp/Texture.hpp>

#include <glint/core/asset.hpp>

namespace glint::core {

class TextureAsset {
  public:
    TextureAsset(const std::string& path, entt::registry& reg);

    static auto extensions() -> std::vector<std::string>;
    static auto load(const std::string& path, entt::registry& reg) -> TextureAsset;

    auto reload(this TextureAsset& self, const std::string& path, entt::registry&) -> void;

    [[nodiscard]]
    auto texture() const -> ::Texture2D;

  private:
    raylib::Texture2D m_texture;
};

static_assert(is_asset<TextureAsset>);

auto setup_textures(entt::registry& reg) -> void;
auto load_texture(entt::registry& reg, const std::string& path) -> entt::resource<TextureAsset>;

} // namespace glint::core

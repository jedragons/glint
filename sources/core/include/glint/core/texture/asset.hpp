#pragma once

#include <string>
#include <vector>

#include <entt/fwd.hpp>
#include <raylib.h>

#include <glint/core/assets/asset.hpp>

namespace glint::core {

class TextureAsset {
  public:
    TextureAsset(const std::string& path, entt::registry& registry);

    static auto extensions() -> std::vector<std::string>;
    static auto load(const std::string& path, entt::registry& registry) -> TextureAsset;

    auto reload(this TextureAsset& self, const std::string& path, entt::registry&) -> void;

    [[nodiscard]]
    auto texture(this const TextureAsset& self) -> Texture2D;

  private:
    Texture2D m_texture {};
};

static_assert(is_asset<TextureAsset>);

} // namespace glint::core

#pragma once

#include <string>
#include <vector>

#include <entt/fwd.hpp>

#include <glint/core/assets/asset.hpp>
#include <glint/platform.hpp>

namespace glint::core {

class TextureAsset {
  public:
    TextureAsset(const std::string& path, entt::registry& registry);

    static auto extensions() -> std::vector<std::string>;
    static auto load(const std::string& path, entt::registry& registry) -> TextureAsset;

    auto reload(this TextureAsset& self, const std::string& path, entt::registry&) -> void;

    [[nodiscard]]
    auto texture(this const TextureAsset& self) -> const Texture&;

  private:
    Texture m_texture {};
};

static_assert(is_asset<TextureAsset>);

} // namespace glint::core

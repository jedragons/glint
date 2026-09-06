#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <raylib.h>

#include <glint/core/assets.hpp>
#include <glint/core/texture.hpp>

namespace glint::dim2 {

struct SpriteFrameMeta {
    uint32_t x {}, y {}, w {}, h {};
    uint32_t duration {};
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SpriteFrameMeta, x, y, w, h, duration);

struct SpriteAtlasMeta {
    std::map<std::string, std::vector<std::string>> animations {};
    std::map<std::string, SpriteFrameMeta> frames {};
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SpriteAtlasMeta, animations, frames);

struct SpriteFrame {
    Rectangle rect {};
    float duration {};
};

struct Sprite {
    core::Resource<core::TextureAsset> texture;
    SpriteFrame frame;

    auto draw(this const Sprite& self, Rectangle dest, Vector2 origin, float rotation, Color tint) -> void;
};

struct AnimatedSprite {
    core::Resource<core::TextureAsset> texture;
    std::vector<SpriteFrame> frames;
    size_t current_frame = 0;
    float time = 0;

    auto frame(this const AnimatedSprite& self) -> std::optional<SpriteFrame>;

    auto update(this AnimatedSprite& self) -> void;

    auto draw(this const AnimatedSprite& self, Rectangle dest, Vector2 origin, float rotation, Color tint) -> void;
};

class AtlasAsset {
  public:
    AtlasAsset(const std::string& path, entt::registry& registry);

    static auto extensions() -> std::vector<std::string>;
    static auto load(const std::string& path, entt::registry& registry) -> AtlasAsset;

    auto reload(this AtlasAsset& self, const std::string& path, entt::registry&) -> void;

    [[nodiscard]]
    auto animation(const std::string& name) -> std::shared_ptr<AnimatedSprite>;
    [[nodiscard]]
    auto sprite(const std::string& name) -> std::shared_ptr<Sprite>;

  private:
    core::Resource<core::TextureAsset> m_texture;
    std::unordered_map<std::string, std::shared_ptr<AnimatedSprite>> m_animations;
    std::unordered_map<std::string, std::shared_ptr<Sprite>> m_sprites;
};

static_assert(core::is_asset<AtlasAsset>);

auto setup_atlases(entt::registry& registry) -> void;
auto load_atlas(entt::registry& registry, const std::string& path) -> core::Resource<AtlasAsset>;

} // namespace glint::dim2

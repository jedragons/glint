#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <raylib-cpp/raylib-cpp.hpp>

#include <glint/core/asset.hpp>
#include <glint/core/asset/texture.hpp>

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
    raylib::Rectangle rect;
    float duration;
};

struct Sprite {
    entt::resource<core::TextureAsset> texture;
    SpriteFrame frame;

    auto draw(
        this const Sprite& self,
        raylib::Rectangle dest,
        raylib::Vector2 origin,
        float rotation,
        raylib::Color tint
    ) -> void;
};

struct AnimatedSprite {
    entt::resource<core::TextureAsset> texture;
    std::vector<SpriteFrame> frames;
    size_t current_frame = 0;
    float time = 0;

    auto frame(this const AnimatedSprite& self) -> std::optional<SpriteFrame>;

    auto update(this AnimatedSprite& self) -> void;

    auto draw(
        this const AnimatedSprite& self,
        raylib::Rectangle dest,
        raylib::Vector2 origin,
        float rotation,
        raylib::Color tint
    ) -> void;
};

class AtlasAsset {
  public:
    AtlasAsset(const std::string& path, entt::registry& reg);

    static auto extensions() -> std::vector<std::string>;
    static auto load(const std::string& path, entt::registry& reg) -> AtlasAsset;

    auto reload(this AtlasAsset& self, const std::string& path, entt::registry&) -> void;

    [[nodiscard]]
    auto animation(const std::string& name) -> std::shared_ptr<AnimatedSprite>;
    [[nodiscard]]
    auto sprite(const std::string& name) -> std::shared_ptr<Sprite>;

  private:
    entt::resource<core::TextureAsset> m_texture;
    std::unordered_map<std::string, std::shared_ptr<AnimatedSprite>> m_animations;
    std::unordered_map<std::string, std::shared_ptr<Sprite>> m_sprites;
};

static_assert(core::is_asset<AtlasAsset>);

auto setup_atlases(entt::registry& reg) -> void;
auto load_atlas(entt::registry& reg, const std::string& path) -> entt::resource<AtlasAsset>;

} // namespace glint::dim2

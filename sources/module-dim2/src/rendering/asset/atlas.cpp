#include <glint/dim2/rendering/asset/atlas.hpp>

#include <filesystem>

#include <glint/core/vfs.hpp>

namespace glint::dim2 {

auto Sprite::draw(this const Sprite& self, Rectangle dest, Vector2 origin, float rotation, Color tint) -> void {
    DrawTexturePro(self.texture->texture(), self.frame.rect, dest, origin, rotation, tint);
}

auto AnimatedSprite::frame(this const AnimatedSprite& self) -> std::optional<SpriteFrame> {
    if (self.frames.empty())
        return std::nullopt;
    else
        return self.frames[self.current_frame];
}

auto AnimatedSprite::update(this AnimatedSprite& self) -> void {
    if (self.frames.empty()) return;

    const auto dt = GetFrameTime();
    self.time += dt;

    if (self.time > self.frames[self.current_frame].duration) {
        self.time = 0.0f;
        self.current_frame += 1;
        if (self.current_frame >= self.frames.size()) {
            self.current_frame = 0;
        }
    }
}

auto AnimatedSprite::draw(this const AnimatedSprite& self, Rectangle dest, Vector2 origin, float rotation, Color tint)
    -> void {
    if (self.frames.empty()) return;

    const auto src = self.frames[self.current_frame].rect;
    DrawTexturePro(self.texture->texture(), src, dest, origin, rotation, tint);
}

AtlasAsset::AtlasAsset(const std::string& path, entt::registry& registry) {
    auto& vfs = core::get_vfs(registry);

    const auto texture_path = std::filesystem::path(path).replace_extension("png").string();
    auto texture = core::load_asset<core::TextureAsset>(registry, texture_path);

    auto json_file = vfs.read(path);
    auto json = nlohmann::json::parse(json_file);
    auto meta = SpriteAtlasMeta(json);

    auto frames = std::unordered_map<std::string, std::shared_ptr<Sprite>> {};
    for (const auto& [name, frame] : meta.frames) {
        frames.insert({
            name,
            std::make_shared<Sprite>(Sprite {
                .texture = texture,
                .frame = SpriteFrame {
                    .rect = Rectangle {float(frame.x), float(frame.y), float(frame.w), float(frame.h)},
                    .duration = float(frame.duration) * 1.0e-3f,
                },
            }),
        });
    }

    auto animations = std::unordered_map<std::string, std::shared_ptr<AnimatedSprite>> {};
    for (const auto& [name, frame_names] : meta.animations) {
        auto sprites = std::vector<SpriteFrame> {};
        for (const auto& frame_name : frame_names) {
            sprites.emplace_back(frames[frame_name]->frame);
        }

        animations.insert(
            {name,
             std::make_shared<AnimatedSprite>(AnimatedSprite {
                 .texture = texture,
                 .frames = sprites,
             })}
        );
    }

    this->m_texture = texture;
    this->m_sprites = std::move(frames);
    this->m_animations = std::move(animations);
}

auto AtlasAsset::extensions() -> std::vector<std::string> {
    return {".json"};
}

auto AtlasAsset::load(const std::string& path, entt::registry& registry) -> AtlasAsset {
    return {path, registry};
}

auto AtlasAsset::reload(this AtlasAsset& self, const std::string& path, entt::registry& registry) -> void {
    auto new_atlas = AtlasAsset {path, registry};
    *self.m_texture = std::move(*new_atlas.m_texture);

    for (auto& [name, sprite] : self.m_sprites) {
        auto new_sprite = new_atlas.m_sprites.find(name);
        if (new_sprite == new_atlas.m_sprites.end()) continue;
        *sprite = *new_sprite->second;
    }

    for (auto& [name, anim] : self.m_animations) {
        auto new_anim = new_atlas.m_animations.find(name);
        if (new_anim == new_atlas.m_animations.end()) continue;
        *anim = *new_anim->second;
    }
}

auto AtlasAsset::animation(const std::string& name) -> std::shared_ptr<AnimatedSprite> {
    return m_animations.at(name);
}

auto AtlasAsset::sprite(const std::string& name) -> std::shared_ptr<Sprite> {
    return m_sprites.at(name);
}

auto setup_atlases(entt::registry& registry) -> void {
    core::register_asset<AtlasAsset>(registry);
}

auto load_atlas(entt::registry& registry, const std::string& path) -> core::Resource<AtlasAsset> {
    return core::load_asset<AtlasAsset>(registry, path);
}

} // namespace glint::dim2

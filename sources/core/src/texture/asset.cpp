#include <glint/core/texture/asset.hpp>

#include <filesystem>

#include <entt/entity/registry.hpp>

#include <glint/core/vfs.hpp>

namespace glint::core {

TextureAsset::TextureAsset(const std::string& path, entt::registry& registry) {
    auto& vfs = get_vfs(registry);
    auto data = vfs.read(path);
    auto ext = std::filesystem::path(path).extension().string();
    m_texture = Texture::load(ext, data);
}

auto TextureAsset::extensions() -> std::vector<std::string> {
    return {".png"};
}

auto TextureAsset::load(const std::string& path, entt::registry& registry) -> TextureAsset {
    return {path, registry};
}

auto TextureAsset::reload(this TextureAsset& self, const std::string& path, entt::registry& registry) -> void {
    self = TextureAsset {path, registry};
}

auto TextureAsset::texture(this const TextureAsset& self) -> const Texture& {
    return self.m_texture;
}

} // namespace glint::core

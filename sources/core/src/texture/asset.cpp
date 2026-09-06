#include <glint/core/texture/asset.hpp>

#include <filesystem>

#include <entt/entity/registry.hpp>

#include <glint/core/vfs.hpp>

namespace glint::core {

TextureAsset::TextureAsset(const std::string& path, entt::registry& registry) {
    auto& vfs = get_vfs(registry);
    auto data = vfs.read(path);
    auto ext = std::filesystem::path(path).extension().string();

    auto image = LoadImageFromMemory(
        ext.c_str(),
        // NOLINTNEXTLINE: Cast from char* to unsigned char* for raylib is safe
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size())
    );
    m_texture = LoadTextureFromImage(image);
    UnloadImage(image);
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

auto TextureAsset::texture(this const TextureAsset& self) -> Texture2D {
    return self.m_texture;
}

} // namespace glint::core

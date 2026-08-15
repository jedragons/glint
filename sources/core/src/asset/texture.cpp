#include <glint/core/asset/texture.hpp>

#include <filesystem>

#include <glint/core/asset_manager.hpp>
#include <glint/core/vfs.hpp>

namespace glint::core {

static auto load_texture(const std::string& path, entt::registry& reg) -> raylib::Texture {
    auto fs_path = std::filesystem::path {path};
    auto ext = fs_path.extension();
    auto& vfs = get_vfs(reg);
    auto data = vfs.read(path);
    auto image = raylib::Image(
        ext,
        // NOLINTNEXTLINE: Cast from char* to unsigned char* for raylib is safe
        reinterpret_cast<unsigned char*>(data.data()),
        static_cast<int>((data.size()))
    );
    return {image};
}

TextureAsset::TextureAsset(const std::string& path, entt::registry& reg) : m_texture(load_texture(path, reg)) {}

auto TextureAsset::extensions() -> std::vector<std::string> {
    return {".png"};
}

auto TextureAsset::load(const std::string& path, entt::registry& reg) -> TextureAsset {
    return {path, reg};
}

auto TextureAsset::reload(this TextureAsset& self, const std::string& path, entt::registry& reg) -> void {
    self.m_texture = load_texture(path, reg);
}

[[nodiscard]]
auto TextureAsset::texture() const -> ::Texture2D {
    return m_texture;
}

auto setup_textures(entt::registry& reg) -> void {
    AssetManager::register_asset<TextureAsset>(reg);
}

auto load_texture(entt::registry& reg, const std::string& path) -> entt::resource<TextureAsset> {
    return AssetManager::load<TextureAsset>(reg, path);
}

} // namespace glint::core

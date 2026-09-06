#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <entt/fwd.hpp>

#include <glint/core/assets/asset.hpp>
#include <glint/core/assets/resource.hpp>

namespace glint::core {

template<is_asset Asset, typename Handle = std::shared_ptr<Asset>>
class AssetCache {
  public:
    auto load(const std::string& path, entt::registry& registry) -> std::pair<Resource<Asset>, bool>;

    [[nodiscard]]
    auto contains(const std::string& path) const -> bool;

    [[nodiscard]]
    auto get(const std::string& path) const -> std::optional<Resource<Asset>>;

  private:
    std::unordered_map<std::string, Resource<Asset>> m_map = {};
};

} // namespace glint::core

// IMPLEMENTATION

namespace glint::core {

template<is_asset Asset, typename Handle>
inline auto AssetCache<Asset, Handle>::load(const std::string& path, entt::registry& registry)
    -> std::pair<Resource<Asset>, bool> {
    if (auto asset = m_map.find(path); asset != m_map.end()) {
        return std::make_pair(asset->second, false);
    }

    auto asset = Resource<Asset>::load(path, registry);
    m_map.insert_or_assign(path, asset);
    return std::make_pair(asset, true);
}

template<is_asset Asset, typename Handle>
inline auto AssetCache<Asset, Handle>::contains(const std::string& path) const -> bool {
    return m_map.contains(path);
}

template<is_asset Asset, typename Handle>
inline auto AssetCache<Asset, Handle>::get(const std::string& path) const -> std::optional<Resource<Asset>> {
    if (auto asset = m_map.find(path); asset != m_map.end()) {
        return asset->second;
    } else {
        return std::nullopt;
    }
}

} // namespace glint::core

#pragma once

#include <entt/fwd.hpp>
#include <string>
#include <vector>

namespace glint::core {

template<typename Asset>
concept is_asset = requires(Asset d, const std::string& path, entt::registry& reg) {
    { Asset::extensions() } -> std::same_as<std::vector<std::string>>;
    { Asset::load(path, reg) } -> std::same_as<Asset>;
    { d.reload(path, reg) } -> std::same_as<void>;
};

} // namespace glint::core

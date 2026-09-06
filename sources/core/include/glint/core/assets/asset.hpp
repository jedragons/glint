#pragma once

#include <concepts>
#include <string>
#include <vector>

#include <entt/fwd.hpp>

namespace glint::core {

template<typename Asset>
concept is_asset = requires(Asset d, const std::string& path, entt::registry& registry) {
    { Asset::extensions() } -> std::same_as<std::vector<std::string>>;
    { Asset::load(path, registry) } -> std::same_as<Asset>;
    { d.reload(path, registry) } -> std::same_as<void>;
};

} // namespace glint::core

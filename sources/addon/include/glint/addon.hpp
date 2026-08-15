#pragma once

#include <functional>
#include <string>
#include <vector>

#include <entt/entt.hpp>

namespace glint {

struct NativeAddon {
    std::string name = {};
    std::vector<std::string> dependencies = {};

    std::function<void(entt::registry&)> pre_init = {};
    std::function<void(entt::registry&)> init = {};
    std::function<void(entt::registry&)> post_init = {};

    std::function<void(entt::registry&)> pre_deinit = {};
    std::function<void(entt::registry&)> deinit = {};
    std::function<void(entt::registry&)> post_deinit = {};

    std::function<void(entt::registry&)> pre_update = {};
    std::function<void(entt::registry&)> update = {};
    std::function<void(entt::registry&)> post_update = {};

    std::function<void(entt::registry&)> pre_render = {};
    std::function<void(entt::registry&)> render = {};
    std::function<void(entt::registry&)> post_render = {};
};

} // namespace glint

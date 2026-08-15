#include <glint/core/vfs.hpp>

#include <raylib-cpp/raylib.hpp>

#include <glint/core/config.hpp>

namespace glint::core {

auto init_vfs(entt::registry& reg) -> void {
    const auto& config = reg.ctx().get<Config>();

    auto& vfs = reg.ctx().emplace<std::unique_ptr<vfs::Vfs>>(std::make_unique<vfs::Vfs>());
    for (const auto& mount : config.mount_points) {
        TraceLog(LOG_INFO, "Mounting %s to %s", mount.point.c_str(), mount.root.c_str());
        vfs->mount(mount.point, mount.root);
    }
}

auto deinit_vfs(entt::registry& reg) -> void {
    reg.ctx().erase<std::unique_ptr<vfs::Vfs>>();
}

auto get_vfs(entt::registry& reg) -> vfs::Vfs& {
    return *reg.ctx().get<std::unique_ptr<vfs::Vfs>>();
}

} // namespace glint::core

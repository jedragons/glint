#include <glint/core.hpp>

#include <glint/core/asset_manager.hpp>
#include <glint/core/bindings/color.hpp>
#include <glint/core/bindings/entity.hpp>
#include <glint/core/bindings/rectangle.hpp>
#include <glint/core/bindings/registry.hpp>
#include <glint/core/bindings/stopwatch.hpp>
#include <glint/core/bindings/timer.hpp>
#include <glint/core/bindings/vector2.hpp>
#include <glint/core/bindings/window.hpp>
#include <glint/core/ecs/name_map.hpp>
#include <glint/core/lua.hpp>
#include <glint/core/vfs.hpp>
#include <glint/core/window.hpp>

namespace glint {

using namespace core;

auto init_core(entt::registry& reg, const Config& config) -> void {
    reg.ctx().emplace<Config>(config);

    init_vfs(reg);
    init_window(reg);
    init_name_map(reg);
    init_assets(reg);
    init_lua(reg);
    setup_textures(reg);

    bind_window(reg);
    bind_registry(reg);
    bind_entity(reg);
    bind_stopwatch(reg);
    bind_timer(reg);
    bind_color(reg);
    bind_vector2(reg);
    bind_rectangle(reg);
}

auto deinit_core(entt::registry& reg) -> void {
    reg.clear();
    deinit_window(reg);
}

auto update_core(entt::registry& reg) -> void {
    update_assets(reg);
    update_lua(reg);
}

} // namespace glint

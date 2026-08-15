#include <glint/nuklear/addon.hpp>

#include <raylib-cpp/raylib-cpp.hpp>
#include <raylib-nuklear.h>
#include <sol/sol.hpp>

#include <glint/nuklear.hpp>
#include <glint/nuklear/bindings/ui.hpp>

namespace glint {

using namespace glint::nuklear;

static auto init_nuklear(entt::registry& reg) -> void;
static auto deinit_nuklear(entt::registry& reg) -> void;
static auto update_nuklear(entt::registry& reg) -> void;
static auto render_nuklear(entt::registry& reg) -> void;

auto create_nuklear_addon() -> NativeAddon {
    return {
        .name = "glint-nuklear",
        .init = init_nuklear,
        .deinit = deinit_nuklear,
        .post_update = update_nuklear,
        .post_render = render_nuklear,
    };
}

static auto init_nuklear(entt::registry& reg) -> void {
    if (reg.ctx().contains<nk_context*>()) return;
    nk_context* ctx = InitNuklear(32);
    reg.ctx().emplace<nk_context*>(ctx);
    bind_ui(reg);
}

static auto deinit_nuklear(entt::registry& reg) -> void {
    if (reg.ctx().contains<nk_context*>()) return;

    auto ctx = reg.ctx().get<nk_context*>();
    UnloadNuklear(ctx);

    reg.ctx().erase<nk_context*>();
}

static auto update_nuklear(entt::registry& reg) -> void {
    UpdateNuklear(get_nuklear(reg));
}

static auto render_nuklear(entt::registry& reg) -> void {
    DrawNuklear(get_nuklear(reg));
}

} // namespace glint

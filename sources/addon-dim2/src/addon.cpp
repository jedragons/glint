#include <glint/dim2/addon.hpp>

#include <raylib-cpp/raylib.hpp>

#include <glint/dim2/asset/atlas.hpp>
#include <glint/dim2/bindings/animated_sprite.hpp>
#include <glint/dim2/bindings/atlas.hpp>
#include <glint/dim2/bindings/sprite.hpp>
#include <glint/dim2/bindings/transform.hpp>

namespace glint {

using namespace dim2;

auto create_dim2_addon() -> NativeAddon {
    return {
        .name = "glint-dim2",
        .init = [](entt::registry& reg) -> void {
            TraceLog(LOG_INFO, "GLINT-2D: Setting up atlas asset");
            setup_atlases(reg);

            TraceLog(LOG_INFO, "GLINT-2D: Binding 2D types to Lua");
            bind_animated_sprite(reg);
            bind_sprite(reg);
            bind_transform(reg);
            bind_atlas(reg);
        },
    };
}

} // namespace glint

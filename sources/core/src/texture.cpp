#include <glint/core/texture.hpp>

#include "glint/core/assets/loading.hpp"

namespace glint::core {

static inline auto import_texture(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)config;

    register_asset<TextureAsset>(registry);
}

auto texture_module() -> Module {
    return Module {
        .name = "glint.core.texture",
        .hooks = {.import = import_texture},
    };
}

} // namespace glint::core

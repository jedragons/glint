#include <glint/audio.hpp>

namespace glint {

static inline auto import_audio(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)registry;
    (void)config;

    if (!registry.ctx().contains<AudioDevice>()) {
        registry.ctx().emplace<AudioDevice>(AudioDevice::create());
    }
}

auto audio_module() -> Module {
    return Module {
        .name = "glint.audio",
        .hooks = {.import = import_audio},
    };
}

} // namespace glint

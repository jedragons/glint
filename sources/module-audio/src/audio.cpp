#include <glint/audio.hpp>

#include <raylib.h>

namespace glint {

static inline auto import_audio(entt::registry& registry, const ModuleConfig& config) -> void {
    (void)registry;
    (void)config;

    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(1024);
}

static inline auto unload_audio(entt::registry& registry) -> void {
    (void)registry;

    CloseAudioDevice();
}

auto audio_module() -> Module {
    return Module {
        .name = "glint.audio",
        .hooks = {.import = import_audio, .unload = unload_audio},
    };
}

} // namespace glint

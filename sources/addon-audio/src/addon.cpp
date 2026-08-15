#include <glint/audio/addon.hpp>

#include <raylib-cpp/raylib-cpp.hpp>

namespace glint {

static auto init_audio(entt::registry& reg) -> void {
    if (reg.ctx().contains<raylib::AudioDevice>()) return;
    reg.ctx().emplace<raylib::AudioDevice>();
    SetAudioStreamBufferSizeDefault(1024);
}

static auto deinit_audio(entt::registry& reg) -> void {
    reg.ctx().erase<raylib::AudioDevice>();
}

auto create_audio_addon() -> NativeAddon {
    return {
        .name = "glint-audio",
        .pre_init = init_audio,
        .post_deinit = deinit_audio,
    };
}

} // namespace glint

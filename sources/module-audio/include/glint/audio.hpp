#pragma once

#include <glint/module.hpp>
#include <glint/platform/audio.hpp>

namespace glint {

auto audio_module() -> Module;

auto get_audio_device(entt::registry& registry) -> AudioDevice&;

} // namespace glint

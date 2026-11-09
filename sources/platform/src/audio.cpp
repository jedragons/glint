#include <glint/platform/audio.hpp>

#include <raylib.h>

namespace glint::platform {

auto AudioDevice::create(const AudioConfig& config) -> AudioDevice {
    ::SetAudioStreamBufferSizeDefault(static_cast<int>(config.buffer_size));
    ::InitAudioDevice();

    return AudioDevice {Data {
        .handle = Handle {
            nullptr,
            [](void*) -> void {
                ::CloseAudioDevice();
            },
        },
    }};
}

AudioDevice::AudioDevice(Data&& data) : m(std::move(data)) {}

} // namespace glint::platform

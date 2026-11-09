#pragma once

#include <memory>

#include <glint/platform/audio_config.hpp>

namespace glint::platform {

class AudioDevice {
  public:
    AudioDevice() = delete;

    static auto create(const AudioConfig& config = {}) -> AudioDevice;

  private:
    using Handle = std::unique_ptr<void, void (*)(void*)>;

    struct Data {
        Handle handle;
    } m;

    explicit AudioDevice(Data&& data);
};

} // namespace glint::platform

namespace glint {

using AudioDevice = platform::AudioDevice;

}

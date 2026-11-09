#pragma once

#include <cstdint>

namespace glint::platform {

struct AudioConfig {
    /// Default maximum buffer size
    uint32_t buffer_size = 2048;

    /// Ignored for now
    uint32_t sample_rate = 0;

    /// Ignored for now
    uint32_t channels = 0;
};

} // namespace glint::platform

#pragma once

#include <string>

#include <glint/platform/log_level.hpp>

namespace glint::platform {

struct WindowConfig {
    std::string title = {};
    int screen_width = 800;
    int screen_height = 600;
    int fps = 0;
    bool resizable : 1 = false;
    bool vsync : 1 = false;
};

struct PlatformConfig {
    WindowConfig window;
    LogLevel log_level = LogLevel::Info;
};

} // namespace glint::platform

namespace glint {

using platform::PlatformConfig;
using platform::WindowConfig;

} // namespace glint

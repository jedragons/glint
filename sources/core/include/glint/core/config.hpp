#pragma once

#include <string>
#include <vector>

#include <entt/fwd.hpp>

#include <glint/addon.hpp>

namespace glint::core {

enum class MountKind {
    Native,

    // TODO: Support zip mount
    Zip,
};

struct MountConfig {
    std::string point = {};
    std::string root = {};
    MountKind kind = MountKind::Native;
};

struct WindowConfig {
    std::string title = {};
    int screen_width = 800;
    int screen_height = 600;
    int fps = 0;
    bool resizable : 1 = false;
    bool vsync : 1 = false;
};

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    All,
};

struct Config {
    WindowConfig window = {};
    LogLevel log_level = LogLevel::Info;
    std::string initial_module = {};
    std::vector<NativeAddon> native_addons = {};
    std::vector<MountConfig> mount_points = {};
};

} // namespace glint::core

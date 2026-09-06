#pragma once

#include <glint/core/core.hpp>
#include <glint/module.hpp>

namespace glint {

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

struct Config {
    core::WindowConfig window = {};
    std::string initial_module = {};
    std::vector<MountConfig> mount_points = {};
    std::vector<Module> modules = {};

    static auto create_default(std::string title = "Glint", int screen_width = 800, int screen_height = 600) -> Config;

    static auto create_empty() -> Config;

    auto set_title(std::string title) -> Config&;
    auto set_screen_width(int screen_width) -> Config&;
    auto set_screen_height(int screen_height) -> Config&;
    auto set_window_resizable(bool resizable = true) -> Config&;
    auto set_vsync(bool vsync = true) -> Config&;
    auto set_log_level(core::LogLevel log_level) -> Config&;
    auto set_initial_behavior(std::string name) -> Config&;
    auto add_default_modules() -> Config&;
    auto add_module(Module mod) -> Config&;
    auto add_native_mount_point(std::string point, std::string root) -> Config&;
    auto build() -> Config;
};

auto empty_config() -> Config;
auto default_config(std::string title = "Glint", int screen_width = 800, int screen_height = 600) -> Config;

} // namespace glint

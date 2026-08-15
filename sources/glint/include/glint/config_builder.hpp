#pragma once

#include <glint/addon.hpp>
#include <glint/core/config.hpp>

namespace glint {

struct ConfigBuilder {
    core::Config config = {};

    static auto create_default(std::string title = "", int screen_width = 800, int screen_height = 600)
        -> ConfigBuilder;

    static auto create_empty() -> ConfigBuilder;

    auto set_title(std::string title) -> ConfigBuilder&;
    auto set_screen_width(int screen_width) -> ConfigBuilder&;
    auto set_screen_height(int screen_height) -> ConfigBuilder&;
    auto set_window_resizable(bool resizable = true) -> ConfigBuilder&;
    auto set_vsync(bool vsync = true) -> ConfigBuilder&;
    auto set_log_level(core::LogLevel log_level) -> ConfigBuilder&;
    auto set_initial_module(std::string module) -> ConfigBuilder&;
    auto add_default_native_addons() -> ConfigBuilder&;
    auto add_native_addon(NativeAddon addon) -> ConfigBuilder&;
    auto add_native_mount_point(std::string point, std::string root) -> ConfigBuilder&;
    auto build() -> core::Config;

    operator core::Config();
};

auto empty_config() -> ConfigBuilder;
auto default_config(std::string title = "", int screen_width = 0, int screen_height = 0) -> ConfigBuilder;

} // namespace glint

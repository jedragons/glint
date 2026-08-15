#include <glint/config_builder.hpp>

#include <glint/audio/addon.hpp>
#include <glint/dim2/addon.hpp>
#include <glint/dim3/addon.hpp>
#include <glint/nuklear/addon.hpp>

namespace glint {

auto ConfigBuilder::create_default(std::string title, int screen_width, int screen_height) -> ConfigBuilder {
    auto builder = ConfigBuilder {};

    builder.set_title(std::move(title))
        .set_screen_width(screen_width)
        .set_screen_height(screen_height)
        .add_default_native_addons();

    return builder;
}

auto ConfigBuilder::create_empty() -> ConfigBuilder {
    return {};
}

auto ConfigBuilder::set_title(std::string title) -> ConfigBuilder& {
    config.window.title = title;
    return *this;
}

auto ConfigBuilder::set_screen_width(int screen_width) -> ConfigBuilder& {
    config.window.screen_width = screen_width;
    return *this;
}

auto ConfigBuilder::set_screen_height(int screen_height) -> ConfigBuilder& {
    config.window.screen_height = screen_height;
    return *this;
}

auto ConfigBuilder::set_window_resizable(bool resizable) -> ConfigBuilder& {
    config.window.resizable = resizable;
    return *this;
}

auto ConfigBuilder::set_vsync(bool vsync) -> ConfigBuilder& {
    config.window.vsync = vsync;
    return *this;
}

auto ConfigBuilder::set_log_level(core::LogLevel log_level) -> ConfigBuilder& {
    config.log_level = log_level;
    return *this;
}

auto ConfigBuilder::set_initial_module(std::string module) -> ConfigBuilder& {
    config.initial_module = module;
    return *this;
}

auto ConfigBuilder::add_default_native_addons() -> ConfigBuilder& {
    return add_native_addon(create_audio_addon())
        .add_native_addon(create_nuklear_addon())
        .add_native_addon(create_dim2_addon())
        .add_native_addon(create_dim3_addon());
}

auto ConfigBuilder::add_native_addon(NativeAddon addon) -> ConfigBuilder& {
    config.native_addons.emplace_back(std::move(addon));
    return *this;
}

auto ConfigBuilder::add_native_mount_point(std::string point, std::string root) -> ConfigBuilder& {
    config.mount_points.emplace_back(
        core::MountConfig {
            .point = std::move(point),
            .root = std::move(root),
            .kind = core::MountKind::Native,
        }
    );
    return *this;
}

auto ConfigBuilder::build() -> core::Config {
    return config;
}

ConfigBuilder::operator core::Config() {
    return config;
}

auto empty_config() -> ConfigBuilder {
    return ConfigBuilder::create_empty();
}

auto default_config(std::string title, int screen_width, int screen_height) -> ConfigBuilder {
    return ConfigBuilder::create_default(title, screen_width, screen_height);
}

} // namespace glint

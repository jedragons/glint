#include <glint/config.hpp>

#include <glint/dim2.hpp>
#include <glint/nuklear.hpp>

namespace glint {

auto Config::create_default(std::string title, int screen_width, int screen_height) -> Config {
    auto builder = Config {};

    builder.set_title(std::move(title))
        .set_screen_width(screen_width)
        .set_screen_height(screen_height)
        .add_default_modules();

    return builder;
}

auto Config::create_empty() -> Config {
    return {};
}

auto Config::set_title(std::string title) -> Config& {
    window.title = title;
    return *this;
}

auto Config::set_screen_width(int screen_width) -> Config& {
    window.screen_width = screen_width;
    return *this;
}

auto Config::set_screen_height(int screen_height) -> Config& {
    window.screen_height = screen_height;
    return *this;
}

auto Config::set_window_resizable(bool resizable) -> Config& {
    window.resizable = resizable;
    return *this;
}

auto Config::set_vsync(bool vsync) -> Config& {
    window.vsync = vsync;
    return *this;
}

auto Config::set_log_level(core::LogLevel log_level) -> Config& {
    log_level = log_level;
    return *this;
}

auto Config::set_initial_behavior(std::string name) -> Config& {
    initial_module = name;
    return *this;
}

auto Config::add_default_modules() -> Config& {
    add_module(core_module());
    add_module(dim2_module());
    add_module(nuklear_module());
    return *this;
}

auto Config::add_module(Module mod) -> Config& {
    modules.emplace_back(mod);
    return *this;
}

auto Config::add_native_mount_point(std::string point, std::string root) -> Config& {
    mount_points.emplace_back(
        MountConfig {
            .point = std::move(point),
            .root = std::move(root),
            .kind = MountKind::Native,
        }
    );
    return *this;
}

auto Config::build() -> Config {
    return {
        .window = window,
        .initial_module = initial_module,
    };
}

auto empty_config() -> Config {
    return Config::create_empty();
}

auto default_config(std::string title, int screen_width, int screen_height) -> Config {
    return Config::create_default(title, screen_width, screen_height);
}

} // namespace glint

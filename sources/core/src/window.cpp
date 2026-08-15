#include <glint/core/window.hpp>

#include <glint/core/config.hpp>

namespace glint::core {

static auto convert_log_level(LogLevel level) -> TraceLogLevel;

auto init_window(entt::registry& reg) -> void {
    const auto& config = reg.ctx().get<Config>();
    const auto& window = config.window;

    auto flags = 0;
    if (window.resizable) flags |= FLAG_WINDOW_RESIZABLE;
    if (window.vsync) flags |= FLAG_VSYNC_HINT;

    reg.ctx().emplace<raylib::Window>(
        window.screen_width,
        window.screen_height,
        window.title,
        flags,
        convert_log_level(config.log_level)
    );

    SetTargetFPS(window.fps);
}

auto deinit_window(entt::registry& reg) -> void {
    reg.ctx().erase<raylib::Window>();
}

auto get_window(entt::registry& reg) -> raylib::Window& {
    return reg.ctx().get<raylib::Window>();
}

static auto convert_log_level(LogLevel level) -> TraceLogLevel {
    switch (level) {
        case LogLevel::Trace:
            return LOG_TRACE;
        case LogLevel::Debug:
            return LOG_DEBUG;
        case LogLevel::Info:
            return LOG_INFO;
        case LogLevel::Warn:
            return LOG_WARNING;
        case LogLevel::Error:
            return LOG_ERROR;
        case LogLevel::All:
        default:
            return LOG_ALL;
    }
}

}; // namespace glint::core


#include <glint/core/windowing.hpp>

#include <raylib.h>

namespace glint::core {

static auto convert_log_level_to_trace_log_level(LogLevel level) -> TraceLogLevel;

Window::Window(const WindowConfig& config) {
    auto flags = 0;
    if (config.resizable) flags |= FLAG_WINDOW_RESIZABLE;
    if (config.vsync) flags |= FLAG_VSYNC_HINT;

    SetTraceLogLevel(convert_log_level_to_trace_log_level(config.log_level));
    SetConfigFlags(flags);
    InitWindow(config.screen_width, config.screen_height, config.title.c_str());
    SetTargetFPS(config.fps);

    m_handle = {std::bit_cast<void*>(1uz), [](void*) -> void { CloseWindow(); }};
}

static auto convert_log_level_to_trace_log_level(LogLevel level) -> TraceLogLevel {
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

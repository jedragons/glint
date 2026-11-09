#include <glint/platform/log.hpp>

#include <raylib.h>

namespace glint::platform {

auto log(LogLevel level, const std::string& message) -> void {
    int raylib_level = {};
    switch (level) {
        case LogLevel::Trace: {
            raylib_level = LOG_TRACE;
            break;
        }
        case LogLevel::Debug: {
            raylib_level = LOG_DEBUG;
            break;
        }
        case LogLevel::Info: {
            raylib_level = LOG_INFO;
            break;
        }
        case LogLevel::Warn: {
            raylib_level = LOG_WARNING;
            break;
        }
        case LogLevel::Error:
        case LogLevel::Fatal: {
            raylib_level = LOG_ERROR;
            break;
        }
    }

    TraceLog(raylib_level, "%s", message.c_str());
}

} // namespace glint::platform

#pragma once

#include <cstdlib>
#include <string>

#include <fmt/format.h>

#include <glint/platform/log_level.hpp>

namespace glint::platform {

auto log(LogLevel level, const std::string& message) -> void;

template<typename... Args>
auto log(LogLevel level, fmt::format_string<Args...> format, Args&&... args) -> void {
    log(level, fmt::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
auto log_trace(fmt::format_string<Args...> format, Args&&... args) -> void {
    log(LogLevel::Trace, fmt::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
auto log_debug(fmt::format_string<Args...> format, Args&&... args) -> void {
    log(LogLevel::Debug, fmt::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
auto log_info(fmt::format_string<Args...> format, Args&&... args) -> void {
    log(LogLevel::Info, fmt::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
auto log_warn(fmt::format_string<Args...> format, Args&&... args) -> void {
    log(LogLevel::Warn, fmt::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
auto log_error(fmt::format_string<Args...> format, Args&&... args) -> void {
    log(LogLevel::Error, fmt::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
[[noreturn]]
auto log_fatal(fmt::format_string<Args...> format, Args&&... args) -> void {
    log(LogLevel::Error, fmt::format(format, std::forward<Args>(args)...));
    std::abort();
}

} // namespace glint::platform

namespace glint {

using platform::log_debug;
using platform::log_error;
using platform::log_fatal;
using platform::log_info;
using platform::log_trace;
using platform::log_warn;

} // namespace glint

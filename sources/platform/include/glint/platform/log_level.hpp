#pragma once

namespace glint::platform {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

}

namespace glint {

using platform::LogLevel;

}

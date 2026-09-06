#pragma once

#include <memory>
#include <string>

namespace glint::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    All,
};

struct WindowConfig {
    std::string title = {};
    int screen_width = 800;
    int screen_height = 600;
    int fps = 0;
    LogLevel log_level = LogLevel::Info;
    bool resizable : 1 = false;
    bool vsync : 1 = false;
};

class Window {
  public:
    Window() = default;
    Window(const WindowConfig& config);

  private:
    std::shared_ptr<void> m_handle;
};

} // namespace glint::core

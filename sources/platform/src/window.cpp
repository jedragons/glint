#include <glint/platform/window.hpp>

#include <raylib.h>

namespace glint::platform {

auto Window::create(const WindowConfig& config) noexcept -> Window {
    return Window {config};
}

auto Window::frame_time() const noexcept -> double {
    return GetFrameTime();
}

auto Window::time() const noexcept -> double {
    return GetTime();
}

auto Window::screen_width() const noexcept -> uint32_t {
    return static_cast<uint32_t>(GetScreenWidth());
}

auto Window::screen_height() const noexcept -> uint32_t {
    return static_cast<uint32_t>(GetScreenHeight());
}

Window::Window(const WindowConfig& config) noexcept {
    unsigned int flags = 0;
    if (config.vsync) flags |= FLAG_VSYNC_HINT;
    if (config.resizable) flags |= FLAG_WINDOW_RESIZABLE;
    SetConfigFlags(flags);

    InitWindow(config.screen_width, config.screen_height, config.title.c_str());
    SetTargetFPS(config.fps);

    m.handle = std::shared_ptr<void>(nullptr, [](void* ptr) -> void {
        CloseWindow();
    });
}

} // namespace glint::platform

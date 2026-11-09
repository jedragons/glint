#pragma once

#include <memory>

#include <glint/platform/window_config.hpp>

namespace glint::platform {

class Window {
  public:
    Window() = delete;

    static auto create(const WindowConfig& config) noexcept -> Window;

    [[nodiscard]]
    auto frame_time() const noexcept -> double;

    [[nodiscard]]
    auto time() const noexcept -> double;

    [[nodiscard]]
    auto screen_width() const noexcept -> uint32_t;

    [[nodiscard]]
    auto screen_height() const noexcept -> uint32_t;

  private:
    struct {
        std::shared_ptr<void> handle;
    } m;

    explicit Window(const WindowConfig& config) noexcept;
};

} // namespace glint::platform

namespace glint {

using platform::Window;

} // namespace glint

#pragma once

namespace glint::core {

class Stopwatch {
  public:
    Stopwatch() = default;

    [[nodiscard]]
    auto elapsed(this const Stopwatch& self) -> float;
    [[nodiscard]]
    auto is_paused(this const Stopwatch& self) -> bool;

    auto reset(this Stopwatch& self) -> Stopwatch&;
    auto set_elapsed(this Stopwatch& self, float elapsed) -> Stopwatch&;
    auto tick(this Stopwatch& self) -> Stopwatch&;
    auto tick(this Stopwatch& self, float dt) -> Stopwatch&;
    auto pause(this Stopwatch& self) -> Stopwatch&;
    auto unpause(this Stopwatch& self) -> Stopwatch&;

  private:
    float m_elapsed = 0.0f;
    bool m_is_paused = false;
};

} // namespace glint::core

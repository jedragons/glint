#pragma once

namespace glint::core {

class StopwatchComp {
  public:
    StopwatchComp() = default;

    [[nodiscard]]
    auto elapsed(this const StopwatchComp& self) -> float;
    [[nodiscard]]
    auto is_paused(this const StopwatchComp& self) -> bool;

    auto reset(this StopwatchComp& self) -> StopwatchComp&;
    auto set_elapsed(this StopwatchComp& self, float elapsed) -> StopwatchComp&;
    auto tick(this StopwatchComp& self) -> StopwatchComp&;
    auto tick(this StopwatchComp& self, float dt) -> StopwatchComp&;
    auto pause(this StopwatchComp& self) -> StopwatchComp&;
    auto unpause(this StopwatchComp& self) -> StopwatchComp&;

  private:
    float m_elapsed = 0.0f;
    bool m_is_paused = false;
};

} // namespace glint::core

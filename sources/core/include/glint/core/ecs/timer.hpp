#pragma once

#include <cstdint>

#include <glint/core/ecs/stopwatch.hpp>

namespace glint::core {

enum class TimerMode : uint8_t {
    Once = 0,
    Repeating = 1,
};

class TimerComp {
  public:
    TimerComp(float duration, TimerMode mode = TimerMode::Once);

    [[nodiscard]]
    auto duration(this const TimerComp& self) -> float;
    [[nodiscard]]
    auto mode(this const TimerComp& self) -> TimerMode;
    [[nodiscard]]
    auto times_finished_this_tick(this const TimerComp& self) -> uint32_t;
    [[nodiscard]]
    auto is_finished(this const TimerComp& self) -> bool;
    [[nodiscard]]
    auto just_finished(this const TimerComp& self) -> bool;
    [[nodiscard]]
    auto is_paused(this const TimerComp& self) -> bool;
    [[nodiscard]]
    auto elapsed(this const TimerComp& self) -> float;
    [[nodiscard]]
    auto remaining(this const TimerComp& self) -> float;
    [[nodiscard]]
    auto fraction(this const TimerComp& self) -> float;

    auto reset(this TimerComp& self) -> TimerComp&;
    auto set_duration(this TimerComp& self, float duration) -> TimerComp&;
    auto set_mode(this TimerComp& self, TimerMode mode) -> TimerComp&;
    auto set_elapsed(this TimerComp& self, float elapsed) -> TimerComp&;
    auto pause(this TimerComp& self) -> TimerComp&;
    auto unpause(this TimerComp& self) -> TimerComp&;
    auto finish(this TimerComp& self) -> TimerComp&;
    auto almost_finish(this TimerComp& self) -> TimerComp&;
    auto tick(this TimerComp& self) -> TimerComp&;
    auto tick(this TimerComp& self, float dt) -> TimerComp&;

  private:
    StopwatchComp m_stopwatch {};
    float m_duration {};
    uint32_t m_times_finished_this_tick {};
    TimerMode m_mode {};
    bool m_finished {};
};

} // namespace glint::core

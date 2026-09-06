#pragma once

#include <cstdint>

#include <glint/core/time/stopwatch.hpp>

namespace glint::core {

enum class TimerMode : uint8_t {
    Once = 0,
    Repeating = 1,
};

class Timer {
  public:
    Timer(float duration, TimerMode mode = TimerMode::Once);

    [[nodiscard]]
    auto duration(this const Timer& self) -> float;
    [[nodiscard]]
    auto mode(this const Timer& self) -> TimerMode;
    [[nodiscard]]
    auto times_finished_this_tick(this const Timer& self) -> uint32_t;
    [[nodiscard]]
    auto is_finished(this const Timer& self) -> bool;
    [[nodiscard]]
    auto just_finished(this const Timer& self) -> bool;
    [[nodiscard]]
    auto is_paused(this const Timer& self) -> bool;
    [[nodiscard]]
    auto elapsed(this const Timer& self) -> float;
    [[nodiscard]]
    auto remaining(this const Timer& self) -> float;
    [[nodiscard]]
    auto fraction(this const Timer& self) -> float;

    auto reset(this Timer& self) -> Timer&;
    auto set_duration(this Timer& self, float duration) -> Timer&;
    auto set_mode(this Timer& self, TimerMode mode) -> Timer&;
    auto set_elapsed(this Timer& self, float elapsed) -> Timer&;
    auto pause(this Timer& self) -> Timer&;
    auto unpause(this Timer& self) -> Timer&;
    auto finish(this Timer& self) -> Timer&;
    auto almost_finish(this Timer& self) -> Timer&;
    auto tick(this Timer& self) -> Timer&;
    auto tick(this Timer& self, float dt) -> Timer&;

  private:
    Stopwatch m_stopwatch {};
    float m_duration {};
    uint32_t m_times_finished_this_tick {};
    TimerMode m_mode {};
    bool m_finished {};
};

} // namespace glint::core

#include <glint/core/time/timer.hpp>

#include <cmath>

#include <raylib.h>

namespace glint::core {

Timer::Timer(float duration, TimerMode mode) : m_duration {duration}, m_mode {mode} {}

auto Timer::mode(this const Timer& self) -> TimerMode {
    return self.m_mode;
}

auto Timer::duration(this const Timer& self) -> float {
    return self.m_duration;
}

auto Timer::times_finished_this_tick(this const Timer& self) -> uint32_t {
    return self.m_times_finished_this_tick;
}

auto Timer::is_finished(this const Timer& self) -> bool {
    return self.m_finished;
}

auto Timer::just_finished(this const Timer& self) -> bool {
    return self.m_times_finished_this_tick > 0;
}

auto Timer::is_paused(this const Timer& self) -> bool {
    return self.m_stopwatch.is_paused();
}

auto Timer::elapsed(this const Timer& self) -> float {
    return self.m_stopwatch.elapsed();
}

auto Timer::remaining(this const Timer& self) -> float {
    return self.duration() - self.elapsed();
}

auto Timer::fraction(this const Timer& self) -> float {
    if (self.m_duration == 0.0f) {
        return 0.0f;
    } else {
        return self.elapsed() / self.duration();
    }
}

auto Timer::reset(this Timer& self) -> Timer& {
    self.m_stopwatch.reset();
    self.m_finished = false;
    self.m_times_finished_this_tick = 0;
    return self;
}

auto Timer::set_duration(this Timer& self, float duration) -> Timer& {
    // TODO: review
    self.m_duration = duration;
    return self;
}

auto Timer::set_mode(this Timer& self, TimerMode mode) -> Timer& {
    // TODO: review
    self.m_mode = mode;
    return self;
}

auto Timer::set_elapsed(this Timer& self, float elapsed) -> Timer& {
    self.m_stopwatch.set_elapsed(elapsed);
    return self;
}

auto Timer::pause(this Timer& self) -> Timer& {
    self.m_stopwatch.pause();
    return self;
}

auto Timer::unpause(this Timer& self) -> Timer& {
    self.m_stopwatch.unpause();
    return self;
}

auto Timer::finish(this Timer& self) -> Timer& {
    self.tick(self.remaining());
    return self;
}

auto Timer::almost_finish(this Timer& self) -> Timer& {
    self.tick(std::nextafterf(self.remaining(), -INFINITY));
    return self;
}

auto Timer::tick(this Timer& self) -> Timer& {
    return self.tick(GetFrameTime());
}

auto Timer::tick(this Timer& self, float dt) -> Timer& {
    if (self.is_paused()) {
        self.m_times_finished_this_tick = 0;
        if (self.m_mode == TimerMode::Repeating) {
            self.m_finished = false;
        }

        return self;
    }

    if (self.m_mode != TimerMode::Repeating && self.is_finished()) {
        self.m_times_finished_this_tick = 0;
        return self;
    }

    self.m_stopwatch.tick(dt);
    self.m_finished = self.elapsed() >= self.duration();

    if (self.is_finished()) {
        if (self.m_mode == TimerMode::Repeating) {
            if (self.duration() == 0.0f) {
                self.m_times_finished_this_tick = UINT32_MAX;
                self.set_elapsed(0.0f);
            } else {
                self.m_times_finished_this_tick = uint32_t(self.elapsed() / self.duration());
                self.set_elapsed(std::remainderf(self.elapsed(), self.duration()));
            }
        } else {
            self.m_times_finished_this_tick = 1;
            self.set_elapsed(self.duration());
        }
    } else {
        self.m_times_finished_this_tick = 0;
    }

    return self;
}

} // namespace glint::core

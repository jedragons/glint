#include <glint/core/ecs/timer.hpp>

#include <raylib-cpp/raylib.hpp>

#include <cmath>

namespace glint::core {

TimerComp::TimerComp(float duration, TimerMode mode) : m_duration {duration}, m_mode {mode} {}

auto TimerComp::mode(this const TimerComp& self) -> TimerMode {
    return self.m_mode;
}

auto TimerComp::duration(this const TimerComp& self) -> float {
    return self.m_duration;
}

auto TimerComp::times_finished_this_tick(this const TimerComp& self) -> uint32_t {
    return self.m_times_finished_this_tick;
}

auto TimerComp::is_finished(this const TimerComp& self) -> bool {
    return self.m_finished;
}

auto TimerComp::just_finished(this const TimerComp& self) -> bool {
    return self.m_times_finished_this_tick > 0;
}

auto TimerComp::is_paused(this const TimerComp& self) -> bool {
    return self.m_stopwatch.is_paused();
}

auto TimerComp::elapsed(this const TimerComp& self) -> float {
    return self.m_stopwatch.elapsed();
}

auto TimerComp::remaining(this const TimerComp& self) -> float {
    return self.duration() - self.elapsed();
}

auto TimerComp::fraction(this const TimerComp& self) -> float {
    if (self.m_duration == 0.0f) {
        return 0.0f;
    } else {
        return self.elapsed() / self.duration();
    }
}

auto TimerComp::reset(this TimerComp& self) -> TimerComp& {
    self.m_stopwatch.reset();
    self.m_finished = false;
    self.m_times_finished_this_tick = 0;
    return self;
}

auto TimerComp::set_duration(this TimerComp& self, float duration) -> TimerComp& {
    // TODO: review
    self.m_duration = duration;
    return self;
}

auto TimerComp::set_mode(this TimerComp& self, TimerMode mode) -> TimerComp& {
    // TODO: review
    self.m_mode = mode;
    return self;
}

auto TimerComp::set_elapsed(this TimerComp& self, float elapsed) -> TimerComp& {
    self.m_stopwatch.set_elapsed(elapsed);
    return self;
}

auto TimerComp::pause(this TimerComp& self) -> TimerComp& {
    self.m_stopwatch.pause();
    return self;
}

auto TimerComp::unpause(this TimerComp& self) -> TimerComp& {
    self.m_stopwatch.unpause();
    return self;
}

auto TimerComp::finish(this TimerComp& self) -> TimerComp& {
    self.tick(self.remaining());
    return self;
}

auto TimerComp::almost_finish(this TimerComp& self) -> TimerComp& {
    self.tick(std::nextafterf(self.remaining(), -INFINITY));
    return self;
}

auto TimerComp::tick(this TimerComp& self) -> TimerComp& {
    return self.tick(GetFrameTime());
}

auto TimerComp::tick(this TimerComp& self, float dt) -> TimerComp& {
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

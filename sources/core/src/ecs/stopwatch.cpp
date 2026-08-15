#include <glint/core/ecs/stopwatch.hpp>

#include <raylib-cpp/raylib.hpp>

namespace glint::core {

auto StopwatchComp::elapsed(this const StopwatchComp& self) -> float {
    return self.m_elapsed;
}

[[nodiscard]]
auto StopwatchComp::is_paused(this const StopwatchComp& self) -> bool {
    return self.m_is_paused;
}

auto StopwatchComp::reset(this StopwatchComp& self) -> StopwatchComp& {
    self.m_elapsed = 0.0f;
    return self;
}

auto StopwatchComp::set_elapsed(this StopwatchComp& self, float elapsed) -> StopwatchComp& {
    self.m_elapsed = elapsed;
    return self;
}

auto StopwatchComp::tick(this StopwatchComp& self) -> StopwatchComp& {
    if (!self.m_is_paused) self.m_elapsed += GetFrameTime();
    return self;
}

auto StopwatchComp::tick(this StopwatchComp& self, float dt) -> StopwatchComp& {
    if (!self.m_is_paused) self.m_elapsed += dt;
    return self;
}

auto StopwatchComp::pause(this StopwatchComp& self) -> StopwatchComp& {
    self.m_is_paused = true;
    return self;
}

auto StopwatchComp::unpause(this StopwatchComp& self) -> StopwatchComp& {
    self.m_is_paused = false;
    return self;
}

} // namespace glint::core

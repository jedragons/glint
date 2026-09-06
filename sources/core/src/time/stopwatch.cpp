#include <glint/core/time/stopwatch.hpp>

#include <raylib.h>

namespace glint::core {

auto Stopwatch::elapsed(this const Stopwatch& self) -> float {
    return self.m_elapsed;
}

[[nodiscard]]
auto Stopwatch::is_paused(this const Stopwatch& self) -> bool {
    return self.m_is_paused;
}

auto Stopwatch::reset(this Stopwatch& self) -> Stopwatch& {
    self.m_elapsed = 0.0f;
    return self;
}

auto Stopwatch::set_elapsed(this Stopwatch& self, float elapsed) -> Stopwatch& {
    self.m_elapsed = elapsed;
    return self;
}

auto Stopwatch::tick(this Stopwatch& self) -> Stopwatch& {
    if (!self.m_is_paused) self.m_elapsed += GetFrameTime();
    return self;
}

auto Stopwatch::tick(this Stopwatch& self, float dt) -> Stopwatch& {
    if (!self.m_is_paused) self.m_elapsed += dt;
    return self;
}

auto Stopwatch::pause(this Stopwatch& self) -> Stopwatch& {
    self.m_is_paused = true;
    return self;
}

auto Stopwatch::unpause(this Stopwatch& self) -> Stopwatch& {
    self.m_is_paused = false;
    return self;
}

} // namespace glint::core

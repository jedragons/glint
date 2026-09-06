#include <glint/core/time/bindings.hpp>

#include <entt/entity/handle.hpp>

#include <glint/core/lua.hpp>
#include <glint/core/time.hpp>

namespace glint::core {

auto bind_stopwatch(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_usertype<Stopwatch>(
        /*@
          ---@class Stopwatch
          ---@field elapsed number
          ---@field new fun(): Stopwatch
          Stopwatch = {}
        */
        "Stopwatch",

        /*@
          ---@return Stopwatch
          function Stopwatch.new() end
        */
        sol::constructors<Stopwatch()>(),

        "elapsed",
        sol::property(&Stopwatch::elapsed, &Stopwatch::set_elapsed),

        /*@
          ---@overload fun(e: Entity): Stopwatch
          ---@param e Entity
          ---@param sw Stopwatch
          ---@return Stopwatch
          function Stopwatch.set(e, sw) end
        */
        "set",
        sol::overload(
            [](entt::handle& h, const Stopwatch& sw) -> Stopwatch& { return h.emplace<Stopwatch>(sw); },
            [](entt::handle& h) -> Stopwatch& { return h.emplace<Stopwatch>(); }
        ),

        /*@
          ---@param e Entity
          ---@return Stopwatch
          function Stopwatch.get(e) end

          ---@param e Entity
          ---@return boolean
          function Stopwatch.has(e) end

          ---@param e Entity
          function Stopwatch.remove(e) end
        */
        GLINT_COMPONENT_FUNCS(Stopwatch),

        /*@
          ---@return boolean
          function Stopwatch:is_paused() end
        */
        "is_paused",
        &Stopwatch::is_paused,

        /*@
          ---@return Stopwatch
          function Stopwatch:reset() end
        */
        "reset",
        &Stopwatch::reset,

        /*@
          ---@overload fun(self: Stopwatch, dt: number): Stopwatch
          ---@return Stopwatch
          function Stopwatch:tick() end
        */
        "tick",
        sol::overload(
            [](Stopwatch& self) -> Stopwatch& {
                return self.tick();
            },
            [](Stopwatch& self, float dt) -> Stopwatch& {
                return self.tick(dt);
            }
        ),

        /*@
          ---@return Stopwatch
          function Stopwatch:pause() end
        */
        "pause",
        &Stopwatch::pause,

        /*@
          ---@return Stopwatch
          function Stopwatch:unpause() end
        */
        "unpause",
        &Stopwatch::unpause
    );
}

auto bind_timer(entt::registry& registry) -> void {
    auto lua = get_lua(registry);

    lua.new_enum(
        /*@
          ---@class TimerMode
          ---@field once integer
          ---@field repeating integer
          TimerMode = {}
        */
        "TimerMode",

        "once",
        int(TimerMode::Once),

        "repeating",
        int(TimerMode::Repeating)
    );

    lua.new_usertype<Timer>(
        /*@
          ---@class Timer
          ---@field duration number
          ---@field mode integer
          ---@field times_finished_this_tick integer
          ---@field is_finished boolean
          ---@field just_finished boolean
          ---@field is_paused boolean
          ---@field elapsed number
          ---@field remaining number
          ---@field fraction number
          Timer = {}
        */
        "Timer",

        /*@
          ---@param duration number
          ---@param mode integer
          ---@return Timer
          function Timer.new(duration, mode) end
        */
        sol::constructors<Timer(float, TimerMode)>(),

        "duration",
        sol::property(&Timer::duration, &Timer::set_duration),

        "mode",
        sol::property(&Timer::mode, &Timer::set_mode),

        "elapsed",
        sol::property(&Timer::elapsed, &Timer::set_elapsed),

        "times_finished_this_tick",
        sol::property(&Timer::times_finished_this_tick),

        "is_finished",
        sol::property(&Timer::is_finished),

        "just_finished",
        sol::property(&Timer::just_finished),

        "is_paused",
        sol::property(&Timer::is_paused),

        "remaining",
        sol::property(&Timer::remaining),

        "fraction",
        sol::property(&Timer::fraction),

        /*@
          ---@overload fun(e: Entity, timer: Timer)
          ---@param e Entity
          ---@param duration number
          ---@param mode integer
          ---@return Timer
          function Timer.set(e, duration, mode) end
        */
        "set",
        sol::overload(
            [](entt::handle& h, float duration, TimerMode mode) -> void { h.emplace<Timer>(duration, mode); },
            [](entt::handle& h, const Timer& timer) -> void { h.emplace<Timer>(timer); }
        ),

        /*@
          ---@param e Entity
          ---@return Timer
          function Timer.get(e) end

          ---@param e Entity
          ---@return boolean
          function Timer.has(e) end

          ---@param e Entity
          function Timer.remove(e) end
        */
        GLINT_COMPONENT_FUNCS(Timer),

        /*@
          function Timer:reset() end
        */
        "reset",
        &Timer::reset,

        /*@
          function Timer:pause() end
        */
        "pause",
        &Timer::pause,

        /*@
          function Timer:unpause() end
        */
        "unpause",
        &Timer::unpause,

        /*@
          function Timer:finish() end
        */
        "finish",
        &Timer::finish,

        /*@
          function Timer:almost_finish() end
        */
        "almost_finish",
        &Timer::almost_finish,

        /*@
          ---@overload fun(self: Timer, dt: number)
          ---@return Timer
          function Timer:tick() end
        */
        "tick",
        sol::overload(
            [](Timer& self) -> Timer& {
                return self.tick();
            },
            [](Timer& self, float dt) -> Timer& {
                return self.tick(dt);
            }
        )
    );
}

} // namespace glint::core

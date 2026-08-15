#include <glint/core/bindings/timer.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/ecs/timer.hpp>
#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_timer(entt::registry& reg) -> void {
    auto& lua = get_lua(reg);

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

    lua.new_usertype<TimerComp>(
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
        sol::constructors<TimerComp(float, TimerMode)>(),

        "duration",
        sol::property(&TimerComp::duration, &TimerComp::set_duration),

        "mode",
        sol::property(&TimerComp::mode, &TimerComp::set_mode),

        "elapsed",
        sol::property(&TimerComp::elapsed, &TimerComp::set_elapsed),

        "times_finished_this_tick",
        sol::property(&TimerComp::times_finished_this_tick),

        "is_finished",
        sol::property(&TimerComp::is_finished),

        "just_finished",
        sol::property(&TimerComp::just_finished),

        "is_paused",
        sol::property(&TimerComp::is_paused),

        "remaining",
        sol::property(&TimerComp::remaining),

        "fraction",
        sol::property(&TimerComp::fraction),

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
            [](entt::handle& handle, float duration, TimerMode mode) -> TimerComp& {
                return handle.emplace_or_replace<TimerComp>(duration, mode);
            },
            [](entt::handle& handle, const TimerComp& timer) -> TimerComp& {
                return handle.emplace_or_replace<TimerComp>(timer);
            }
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
        GLINT_COMPONENT_FUNCS(TimerComp),

        /*@
          function Timer:reset() end
        */
        "reset",
        &TimerComp::reset,

        /*@
          function Timer:pause() end
        */
        "pause",
        &TimerComp::pause,

        /*@
          function Timer:unpause() end
        */
        "unpause",
        &TimerComp::unpause,

        /*@
          function Timer:finish() end
        */
        "finish",
        &TimerComp::finish,

        /*@
          function Timer:almost_finish() end
        */
        "almost_finish",
        &TimerComp::almost_finish,

        /*@
          ---@overload fun(self: Timer, dt: number)
          ---@return Timer
          function Timer:tick() end
        */
        "tick",
        sol::overload(
            [](TimerComp& self) -> TimerComp& { return self.tick(); },
            [](TimerComp& self, float dt) -> TimerComp& { return self.tick(dt); }
        )
    );
}

} // namespace glint::core

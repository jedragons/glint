#include <glint/core/bindings/stopwatch.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#include <glint/core/ecs/stopwatch.hpp>
#include <glint/core/lua.hpp>

namespace glint::core {

auto bind_stopwatch(entt::registry& reg) -> void {
    auto& lua = get_lua(reg);

    lua.new_usertype<StopwatchComp>(
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
        sol::constructors<StopwatchComp()>(),

        "elapsed",
        sol::property(&StopwatchComp::elapsed, &StopwatchComp::set_elapsed),

        /*@
          ---@overload fun(e: Entity): Stopwatch
          ---@param e Entity
          ---@param sw Stopwatch
          ---@return Stopwatch
          function Stopwatch.set(e, sw) end
        */
        "set",
        sol::overload(
            [](entt::handle& e, const StopwatchComp& sw) -> StopwatchComp& {
                return e.emplace_or_replace<StopwatchComp>(sw);
            },
            [](entt::handle& e) -> StopwatchComp& { return e.emplace_or_replace<StopwatchComp>(); }
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
        GLINT_COMPONENT_FUNCS(StopwatchComp),

        /*@
          ---@return boolean
          function Stopwatch:is_paused() end
        */
        "is_paused",
        &StopwatchComp::is_paused,

        /*@
          ---@return Stopwatch
          function Stopwatch:reset() end
        */
        "reset",
        &StopwatchComp::reset,

        /*@
          ---@overload fun(self: Stopwatch, dt: number): Stopwatch
          ---@return Stopwatch
          function Stopwatch:tick() end
        */
        "tick",
        sol::overload(
            [](StopwatchComp& self) -> StopwatchComp& { return self.tick(); },
            [](StopwatchComp& self, float dt) -> StopwatchComp& { return self.tick(dt); }
        ),

        /*@
          ---@return Stopwatch
          function Stopwatch:pause() end
        */
        "pause",
        &StopwatchComp::pause,

        /*@
          ---@return Stopwatch
          function Stopwatch:unpause() end
        */
        "unpause",
        &StopwatchComp::unpause
    );
}

} // namespace glint::core

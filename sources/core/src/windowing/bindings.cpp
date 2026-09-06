#include <glint/core/windowing/bindings.hpp>

#include <sstream>

#include <raylib.h>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>

namespace glint::core {

auto lua_log(TraceLogLevel level, sol::variadic_args va) -> void {
    auto buf = std::stringstream {};
    for (const auto arg : va) {
        auto str = lua_tostring(va.lua_state(), arg.stack_index());
        if (str == nullptr) str = "<unknown>";
        buf << str << " ";
    }

    auto message = buf.str();
    if (!message.empty() && message.back() == ' ') {
        message.pop_back();
    }

    TraceLog(level, "%s", message.c_str());
}

auto bind_window(entt::registry& registry) -> void {
    auto lua = core::get_lua(registry);

    /*@
      ---@return number
      function get_dt() end
     */
    lua.globals()["get_dt"] = []() -> float { return GetFrameTime(); };

    /*@
      ---@return integer
      function get_screen_width() end
     */
    lua.globals()["get_screen_width"] = []() -> int { return GetScreenWidth(); };

    /*@
      ---@return integer
      function get_screen_height() end
     */
    lua.globals()["get_screen_height"] = []() -> int { return GetScreenHeight(); };

    /*@
      ---@param ... any
      function log_trace(...) end
     */
    lua.globals()["log_trace"] = [](sol::variadic_args va) -> void { lua_log(LOG_TRACE, va); };

    /*@
      ---@param ... any
      function log_debug(...) end
     */
    lua.globals()["log_debug"] = [](sol::variadic_args va) -> void { lua_log(LOG_DEBUG, va); };

    /*@
      ---@param ... any
      function log_info(...) end
     */
    lua.globals()["log_info"] = [](sol::variadic_args va) -> void { lua_log(LOG_INFO, va); };

    /*@
      ---@param ... any
      function log_warn(...) end
     */
    lua.globals()["log_warn"] = [](sol::variadic_args va) -> void { lua_log(LOG_WARNING, va); };

    /*@
      ---@param ... any
      function log_error(...) end
     */
    lua.globals()["log_error"] = [](sol::variadic_args va) -> void { lua_log(LOG_ERROR, va); };

    /*@
      ---@param ... any
      function log_fatal(...) end
     */
    lua.globals()["log_fatal"] = [](sol::variadic_args va) -> void {
        lua_log(LOG_ERROR, va);
        throw sol::error("log_fatal called");
    };
}

} // namespace glint::core

#include <glint/core/windowing/bindings.hpp>

#include <sstream>

#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/core/windowing.hpp>

namespace glint::core {

auto lua_log(LogLevel level, sol::variadic_args va) -> void {
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

    log(level, "{}", message);
}

auto bind_window(entt::registry& registry) -> void {
    auto lua = core::get_lua(registry);
    auto& window = get_window(registry);

    lua.globals().set("window", window);

    lua.new_usertype<Window>(
        /*@
          ---@class Window
          ---@field dt number
          ---@field time number
          ---@field width integer
          ---@field height integer
          Window = {}
        */
        "Window",

        "dt",
        sol::property([window]() -> double {
            return window.frame_time();
        }),

        "time",
        sol::property([window]() -> double {
            return window.time();
        }),

        "width",
        sol::property([window]() -> uint32_t {
            return window.screen_width();
        }),

        "height",
        sol::property([window]() -> uint32_t {
            return window.screen_height();
        })
    );

    // NOTE: The unary + on each lambda forces conversion to a plain function
    // pointer. mingw-g++ miscompiles bare capture-less lambdas registered via
    // sol2: every `globals().set(name, lambda)` ends up pointing at the first
    // registered closure, so all log_* calls landed in log_trace and were
    // silently dropped by raylib's log threshold (only seen on Windows/wine).

    /*@
      ---@param ... any
      function log_trace(...) end
     */
    lua.globals().set("log_trace", +[](sol::variadic_args va) {
        lua_log(LogLevel::Trace, va);
    });

    /*@
      ---@param ... any
      function log_debug(...) end
     */
    lua.globals().set("log_debug", +[](sol::variadic_args va) {
        lua_log(LogLevel::Debug, va);
    });

    /*@
      ---@param ... any
      function log_info(...) end
     */
    lua.globals().set("log_info", +[](sol::variadic_args va) {
        lua_log(LogLevel::Info, va);
    });

    /*@
      ---@param ... any
      function log_warn(...) end
     */
    lua.globals().set("log_warn", +[](sol::variadic_args va) {
        lua_log(LogLevel::Warn, va);
    });

    /*@
      ---@param ... any
      function log_error(...) end
     */
    lua.globals().set("log_error", +[](sol::variadic_args va) {
        lua_log(LogLevel::Error, va);
    });

    /*@
      ---@param ... any
      function log_fatal(...) end
     */
    lua.globals().set("log_fatal", +[](sol::variadic_args va) {
        lua_log(LogLevel::Error, va);
        throw sol::error("log_fatal called");
    });
}

} // namespace glint::core

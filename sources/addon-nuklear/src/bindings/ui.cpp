#include <glint/nuklear/bindings/ui.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>
#include <raylib-cpp/Color.hpp>
#include <raylib-cpp/Rectangle.hpp>
#include <raylib-nuklear.h>
#include <sol/sol.hpp>

#include <glint/core/lua.hpp>
#include <glint/dim2/ecs/sprite.hpp>
#include <glint/nuklear.hpp>

namespace glint::nuklear {

static auto bind_globals(entt::registry& reg, sol::state& lua) -> void;
static auto bind_flags(entt::registry& reg, sol::state& lua) -> void;
static auto bind_ui(entt::registry& reg, sol::state& lua) -> void;
static auto bind_window(entt::registry& reg, sol::state& lua) -> void;
static auto bind_canvas(entt::registry& reg, sol::state& lua) -> void;

auto bind_ui(entt::registry& reg) -> void {
    auto& lua = core::get_lua(reg);
    bind_globals(reg, lua);
    bind_flags(reg, lua);
    bind_ui(reg, lua);
    bind_window(reg, lua);
    bind_canvas(reg, lua);
}

static auto bind_globals(entt::registry& reg, sol::state& lua) -> void {
    /*@
      ---@return Ui
      function ui() end
     */
    lua.globals()["ui"] = [&reg]() -> nk_context& { return *reg.ctx().get<nk_context*>(); };
}

static auto bind_flags(entt::registry&, sol::state& lua) -> void {
    lua.new_enum(
        /*@
          ---@class UiFlagsTable
          ---@field background integer
          ---@field border integer
          ---@field closable integer
          ---@field closed integer
          ---@field dynamic integer
          ---@field hidden integer
          ---@field minimizable integer
          ---@field minimized integer
          ---@field movable integer
          ---@field no_input integer
          ---@field no_scrollbar integer
          ---@field not_interactive integer
          ---@field private integer
          ---@field rom integer
          ---@field scalable integer
          ---@field scale_left integer
          ---@field scroll_auto_hide integer
          ---@field title integer
          UiFlags = {}
         */
        "UiFlags",

        "background",
        NK_WINDOW_BACKGROUND,

        "border",
        NK_WINDOW_BORDER,

        "closable",
        NK_WINDOW_CLOSABLE,

        "closed",
        NK_WINDOW_CLOSED,

        "dynamic",
        NK_WINDOW_DYNAMIC,

        "hidden",
        NK_WINDOW_HIDDEN,

        "minimizable",
        NK_WINDOW_MINIMIZABLE,

        "minimized",
        NK_WINDOW_MINIMIZED,

        "movable",
        NK_WINDOW_MOVABLE,

        "no_input",
        NK_WINDOW_NO_INPUT,

        "no_scrollbar",
        NK_WINDOW_NO_SCROLLBAR,

        "not_interactive",
        NK_WINDOW_NOT_INTERACTIVE,

        "private",
        NK_WINDOW_PRIVATE,

        "rom",
        NK_WINDOW_ROM,

        "scalable",
        NK_WINDOW_SCALABLE,

        "scale_left",
        NK_WINDOW_SCALE_LEFT,

        "scroll_auto_hide",
        NK_WINDOW_SCROLL_AUTO_HIDE,

        "title",
        NK_WINDOW_TITLE
    );
}

static auto bind_ui(entt::registry&, sol::state& lua) -> void {
    lua.new_usertype<nk_context>(
        /*@
          ---@class Ui
          Ui = {}
         */
        "Ui",

        /*@
          ---@param title string
          ---@param bounds Rect
          ---@param flags integer
          ---@param body fun(window: UiWindow)
          function Ui:window(title, bounds, flags, body) end
         */
        "window",
        [](nk_context& ctx, const std::string& title, raylib::Rectangle bounds, unsigned int flags, sol::function func)
            -> void {
            auto result = sol::protected_function_result {};
            if (nk_begin(&ctx, title.c_str(), RectangleToNuklearRect(&ctx, bounds), flags)) {
                result = func(*ctx.current);
            }
            nk_end(&ctx);
            if (!result.valid()) throw sol::error {result};
        },
        /*@
          ---@param name string
          ---@param title string
          ---@param bounds Rect
          ---@param flags integer
          ---@param body fun(window: UiWindow)
          function Ui:window_titled(title, bounds, flags, body) end
         */
        "window_titled",
        [](nk_context& ctx,
           const std::string& name,
           const std::string& title,
           raylib::Rectangle bounds,
           unsigned int flags,
           sol::safe_function func) -> void {
            auto result = sol::protected_function_result {};
            if (nk_begin_titled(&ctx, name.c_str(), title.c_str(), RectangleToNuklearRect(&ctx, bounds), flags)) {
                result = func(*ctx.current);
            }
            nk_end(&ctx);
            if (!result.valid()) throw sol::error {result};
        },

        /*@
          ---@param height number
          ---@param item_width integer
          ---@param cols integer
          function Ui:layout_row_static(height, item_width, cols) end
         */
        "layout_row_static",
        [](nk_context& ctx, float height, int item_width, int cols) -> void {
            nk_layout_row_static(&ctx, height, item_width, cols);
        },

        /*@
          ---@param height number
          ---@param cols integer
          function Ui:layout_row_dynamic(height, cols) end
         */
        "layout_row_dynamic",
        [](nk_context& ctx, float height, int cols) -> void { nk_layout_row_dynamic(&ctx, height, cols); },

        /*@
          ---@param label string
          ---@return boolean
          function Ui:button_label(label) end
         */
        "button_label",
        [](nk_context& ctx, const std::string& label) -> bool { return nk_button_label(&ctx, label.c_str()); }
    );
}

static auto bind_window(entt::registry& reg, sol::state& lua) -> void {
    lua.new_usertype<nk_window>(
        /*@
          ---@class UiWindow
          ---@field bounds Rect
          ---@field canvas UiCanvas
          UiWindow = {}
         */
        "UiWindow",

        "bounds",
        sol::property([&reg](nk_window& w) -> raylib::Rectangle {
            return NuklearRectToRectangle(get_nuklear(reg), w.bounds);
        }),

        "canvas",
        sol::property([](nk_window& w) -> nk_command_buffer* { return &w.buffer; })
    );
}

static auto bind_canvas(entt::registry& reg, sol::state& lua) -> void {
    lua.new_usertype<nk_command_buffer>(
        /*@
          ---@class UiCanvas
          UiCanvas = {}
         */
        "UiCanvas",

        /*@
          ---@param x0 number
          ---@param y0 number
          ---@param x1 number
          ---@param y1 number
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_line(x0, y0, x1, y1, line_thickness, color) end
         */
        "stroke_line",
        [](nk_command_buffer& b, float x0, float y0, float x1, float y1, float line_thickness, raylib::Color color)
            -> void { nk_stroke_line(&b, x0, y0, x1, y1, line_thickness, ColorToNuklearColor(color)); },

        /*@
          ---@param ax number
          ---@param ay number
          ---@param ctrl0x number
          ---@param ctrl0y number
          ---@param ctrl1x number
          ---@param ctrl1y number
          ---@param bx number
          ---@param by number
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_curve(ax, ay, ctrl0x, ctrl0y, ctrl1x, ctrl1y, bx, by, line_thickness, color) end
         */
        "stroke_curve",
        [](nk_command_buffer& b,
           float ax,
           float ay,
           float ctrl0x,
           float ctrl0y,
           float ctrl1x,
           float ctrl1y,
           float bx,
           float by,
           float line_thickness,
           raylib::Color color) -> void {
            const auto col = ColorToNuklearColor(color);
            nk_stroke_curve(&b, ax, ay, ctrl0x, ctrl0y, ctrl1x, ctrl1y, bx, by, line_thickness, col);
        },

        /*@
          ---@param rect Rect
          ---@param rounding number
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_rect(rect, rounding, line_thickness, color) end
         */
        "stroke_rect",
        [&reg](nk_command_buffer& b, raylib::Rectangle rect, float rounding, float line_thickness, raylib::Color color)
            -> void {
            const auto nk_rect = RectangleToNuklearRect(get_nuklear(reg), rect);
            const auto col = ColorToNuklearColor(color);
            nk_stroke_rect(&b, nk_rect, rounding, line_thickness, col);
        },

        /*@
          ---@param rect Rect
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_circle(rect, line_thickness, color) end
         */
        "stroke_circle",
        [&reg](nk_command_buffer& b, raylib::Rectangle rect, float line_thickness, raylib::Color color) -> void {
            const auto nk_rect = RectangleToNuklearRect(get_nuklear(reg), rect);
            const auto col = ColorToNuklearColor(color);
            nk_stroke_circle(&b, nk_rect, line_thickness, col);
        },

        /*@
          ---@param cx number
          ---@param cy number
          ---@param radius number
          ---@param a_min number
          ---@param a_max number
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_arc(cx, cy, radius, a_min, a_max, line_thickness, color) end
         */
        "stroke_arc",
        [](nk_command_buffer& b,
           float cx,
           float cy,
           float radius,
           float a_min,
           float a_max,
           float line_thickness,
           raylib::Color color) -> void {
            const auto col = ColorToNuklearColor(color);
            const auto a_min_rad = a_min * DEG2RAD;
            const auto a_max_rad = a_max * DEG2RAD;
            nk_stroke_arc(&b, cx, cy, radius, a_min_rad, a_max_rad, line_thickness, col);
        },

        /*@
          ---@param x0 number
          ---@param y0 number
          ---@param x1 number
          ---@param y1 number
          ---@param x2 number
          ---@param y2 number
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_triangle(x0, y0, x1, y1, x2, y2, line_thickness, color) end
         */
        "stroke_triangle",
        [](nk_command_buffer& b,
           float x0,
           float y0,
           float x1,
           float y1,
           float x2,
           float y2,
           float line_thickness,
           raylib::Color color) -> void {
            const auto col = ColorToNuklearColor(color);
            nk_stroke_triangle(&b, x0, y0, x1, y1, x2, y2, line_thickness, col);
        },

        /*@
          ---@param points number[]
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_polyline(points, line_thickness, color) end
         */
        "stroke_polyline",
        [](nk_command_buffer& b, sol::table points, float line_thickness, raylib::Color color) -> void {
            const auto col = ColorToNuklearColor(color);
            thread_local auto point_vec = std::vector<float> {};
            const auto count = static_cast<int>(points.size());

            point_vec.clear();
            for (auto i = 1; i <= count; i++) {
                auto point = points.get<std::optional<float>>(i);
                if (!point) throw sol::error("Points must be an array of numbers");
                point_vec.emplace_back(*point);
            }

            nk_stroke_polyline(&b, point_vec.data(), count / 2, line_thickness, col);
        },

        /*@
          ---@param points number[]
          ---@param line_thickness number
          ---@param color Color
          function UiCanvas:stroke_polygon(points, line_thickness, color) end
         */
        "stroke_polygon",
        [](nk_command_buffer& b, sol::table points, float line_thickness, raylib::Color color) -> void {
            const auto col = ColorToNuklearColor(color);
            thread_local auto point_vec = std::vector<float> {};
            const auto count = static_cast<int>(points.size());

            point_vec.clear();
            for (auto i = 1; i <= count; i++) {
                auto point = points.get<std::optional<float>>(i);
                if (!point) throw sol::error("Points must be an array of numbers");
                point_vec.emplace_back(*point);
            }

            nk_stroke_polygon(&b, point_vec.data(), count / 2, line_thickness, col);
        },

        /*@
          ---@param rect Rect
          ---@param rounding number
          ---@param color Color
          function UiCanvas:fill_rect(rect, rounding, color) end
         */
        "fill_rect",
        [&reg](nk_command_buffer& b, raylib::Rectangle rect, float rounding, raylib::Color color) -> void {
            const auto nk_rect = RectangleToNuklearRect(get_nuklear(reg), rect);
            const auto col = ColorToNuklearColor(color);
            nk_fill_rect(&b, nk_rect, rounding, col);
        },

        /*@
          ---@param rect Rect
          ---@param left Color
          ---@param top Color
          ---@param right Color
          ---@param bottom Color
          function UiCanvas:fill_rect_multi_color(rect, left, top, right, bottom) end
         */
        "fill_rect_multi_color",
        [&reg](
            nk_command_buffer& b,
            raylib::Rectangle rect,
            raylib::Color left,
            raylib::Color top,
            raylib::Color right,
            raylib::Color bottom
        ) -> void {
            const auto nk_rect = RectangleToNuklearRect(get_nuklear(reg), rect);
            nk_fill_rect_multi_color(
                &b,
                nk_rect,
                ColorToNuklearColor(left),
                ColorToNuklearColor(top),
                ColorToNuklearColor(right),
                ColorToNuklearColor(bottom)
            );
        },

        /*@
          ---@param rect Rect
          ---@param color Color
          function UiCanvas:fill_circle(rect, color) end
         */
        "fill_circle",
        [&reg](nk_command_buffer& b, raylib::Rectangle rect, raylib::Color color) -> void {
            const auto nk_rect = RectangleToNuklearRect(get_nuklear(reg), rect);
            const auto col = ColorToNuklearColor(color);
            nk_fill_circle(&b, nk_rect, col);
        },

        /*@
          ---@param cx number
          ---@param cy number
          ---@param radius number
          ---@param a_min number
          ---@param a_max number
          ---@param color Color
          function UiCanvas:fill_arc(cx, cy, radius, a_min, a_max, color) end
         */
        "fill_arc",
        [](nk_command_buffer& b, float cx, float cy, float radius, float a_min, float a_max, raylib::Color color)
            -> void {
            const auto col = ColorToNuklearColor(color);
            const auto a_min_rad = a_min * DEG2RAD;
            const auto a_max_rad = a_max * DEG2RAD;
            nk_fill_arc(&b, cx, cy, radius, a_min_rad, a_max_rad, col);
        },

        /*@
          ---@param x0 number
          ---@param y0 number
          ---@param x1 number
          ---@param y1 number
          ---@param x2 number
          ---@param y2 number
          ---@param color Color
          function UiCanvas:fill_triangle(x0, y0, x1, y1, x2, y2, color) end
         */
        "fill_triangle",
        [](nk_command_buffer& b, float x0, float y0, float x1, float y1, float x2, float y2, raylib::Color color)
            -> void {
            const auto col = ColorToNuklearColor(color);
            nk_fill_triangle(&b, x0, y0, x1, y1, x2, y2, col);
        },

        /*@
          ---@param points number[]
          ---@param color Color
          function UiCanvas:fill_polygon(points, color) end
         */
        "fill_polygon",
        [](nk_command_buffer& b, sol::table points, raylib::Color color) -> void {
            const auto col = ColorToNuklearColor(color);
            thread_local auto point_vec = std::vector<float> {};
            const auto count = static_cast<int>(points.size());

            point_vec.clear();
            for (auto i = 1; i <= count; i++) {
                auto point = points.get<std::optional<float>>(i);
                if (!point) throw sol::error("Points must be an array of numbers");
                point_vec.emplace_back(*point);
            }

            nk_fill_polygon(&b, point_vec.data(), count / 2, col);
        },

        /*@
          ---@param rect Rect
          ---@param sprite Sprite
          ---@param color Color
          function UiCanvas:draw_image(rect, sprite, color) end
         */
        "draw_image",
        [&reg](nk_command_buffer& b, raylib::Rectangle rect, dim2::SpriteComp sprite, raylib::Color color) -> void {
            const auto& bounds = sprite.sprite->frame.rect;
            auto img = TextureToNuklearImageEx(sprite.sprite->texture->texture(), bounds);
            const auto nk_rect = RectangleToNuklearRect(get_nuklear(reg), rect);
            const auto nk_color = ColorToNuklearColor(color);
            nk_draw_image(&b, nk_rect, &img, nk_color);
        }
    );
}

} // namespace glint::nuklear

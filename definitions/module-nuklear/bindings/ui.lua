---@meta

---@return Ui
function ui() end

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

---@class Ui
Ui = {}

---@param title string
---@param bounds Rect
---@param flags integer
---@param body fun(window: UiWindow)
function Ui:window(title, bounds, flags, body) end

---@param name string
---@param title string
---@param bounds Rect
---@param flags integer
---@param body fun(window: UiWindow)
function Ui:window_titled(name, title, bounds, flags, body) end

---@param height number
---@param item_width integer
---@param cols integer
function Ui:layout_row_static(height, item_width, cols) end

---@param height number
---@param cols integer
function Ui:layout_row_dynamic(height, cols) end

---@param label string
---@return boolean
function Ui:button_label(label) end

---@class UiWindow
---@field bounds Rect
---@field canvas UiCanvas
UiWindow = {}

---@class UiCanvas
UiCanvas = {}

---@param x0 number
---@param y0 number
---@param x1 number
---@param y1 number
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_line(x0, y0, x1, y1, line_thickness, color) end

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

---@param rect Rect
---@param rounding number
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_rect(rect, rounding, line_thickness, color) end

---@param rect Rect
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_circle(rect, line_thickness, color) end

---@param cx number
---@param cy number
---@param radius number
---@param a_min number
---@param a_max number
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_arc(cx, cy, radius, a_min, a_max, line_thickness, color) end

---@param x0 number
---@param y0 number
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_triangle(x0, y0, x1, y1, x2, y2, line_thickness, color) end

---@param points number[]
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_polyline(points, line_thickness, color) end

---@param points number[]
---@param line_thickness number
---@param color Color
function UiCanvas:stroke_polygon(points, line_thickness, color) end

---@param rect Rect
---@param rounding number
---@param color Color
function UiCanvas:fill_rect(rect, rounding, color) end

---@param rect Rect
---@param left Color
---@param top Color
---@param right Color
---@param bottom Color
function UiCanvas:fill_rect_multi_color(rect, left, top, right, bottom) end

---@param rect Rect
---@param color Color
function UiCanvas:fill_circle(rect, color) end

---@param cx number
---@param cy number
---@param radius number
---@param a_min number
---@param a_max number
---@param color Color
function UiCanvas:fill_arc(cx, cy, radius, a_min, a_max, color) end

---@param x0 number
---@param y0 number
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@param color Color
function UiCanvas:fill_triangle(x0, y0, x1, y1, x2, y2, color) end

---@param points number[]
---@param color Color
function UiCanvas:fill_polygon(points, color) end

---@param rect Rect
---@param sprite Sprite
---@param color Color
function UiCanvas:draw_image(rect, sprite, color) end

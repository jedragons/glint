---@meta

---@class Vec2
---@field x number
---@field y number
---@operator add(Vec2): Vec2
---@operator sub(Vec2): Vec2
---@operator mul(Vec2): Vec2
---@operator div(Vec2): Vec2
Vec2 = {}

---@param x? number
---@param y? number
---@return Vec2
function Vec2.new(x, y) end

---@param scale number
---@return Vec2
function Vec2:scale(scale) end

---@class Rect
---@field x number
---@field y number
---@field w number
---@field h number
Rect = {}

---@param x? number
---@param y? number
---@param w? number
---@param h? number
---@return Rect
function Rect.new(x, y, w, h) end

---@class Color
---@field r integer
---@field g integer
---@field b integer
---@field a integer
---@field lightgray Color
---@field gray Color
---@field darkgray Color
---@field yellow Color
---@field gold Color
---@field orange Color
---@field pink Color
---@field red Color
---@field maroon Color
---@field green Color
---@field lime Color
---@field darkgreen Color
---@field skyblue Color
---@field blue Color
---@field darkblue Color
---@field purple Color
---@field violet Color
---@field darkpurple Color
---@field beige Color
---@field brown Color
---@field darkbrown Color
---@field white Color
---@field black Color
---@field blank Color
---@field magenta Color
Color = {}

---@overload fun(hex: integer): Color
---@overload fun(r: integer, g: integer, b: integer): Color
---@overload fun(r: integer, g: integer, b: integer, a: integer): Color
---@return Color
function Color.new() end

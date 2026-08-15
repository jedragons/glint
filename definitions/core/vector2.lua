---@meta

---@class Vec2
---@field x number
---@field y number
---@operator add(Vec2): Vec2
---@operator sub(Vec2): Vec2
---@operator mul(Vec2): Vec2
---@operator div(Vec2): Vec2
Vec2 = {}

---@overload fun(x: number, y: number): Vec2
---@return Vec2
function Vec2.new() end

---@param scale number
---@return Vec2
function Vec2:scale(scale) end

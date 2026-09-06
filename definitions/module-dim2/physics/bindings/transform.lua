---@meta

---@class Transform2D
---@field position Vec2
---@field scale Vec2
---@field origin Vec2
---@field rotation number
Transform2D = {}

---@overload fun(position: Vec2, scale: Vec2, origin: Vec2, rotation: Vec2): Transform2D
---@return Transform2D
function Transform2D.new() end

---@overload fun(e): Transform2D
---@overload fun(e, position: Vec2, scale: Vec2, origin: Vec2, rotation: number): Transform2D
---@param e Entity
---@param comp Transform2D
---@return Transform2D
function Transform2D.set(e, comp) end

---@param e Entity
---@return Transform2D
function Transform2D.get(e) end

---@param e Entity
---@return boolean
function Transform2D.has(e) end

---@param e Entity
function Transform2D.remove(e) end

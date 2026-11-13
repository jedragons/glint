---@meta

---A live reference to a transform vector with Vec2 arithmetic and methods.
---@class TransformVec2: Vec2

---@class Transform2D
---@field position TransformVec2
---@field scale TransformVec2
---@field origin TransformVec2
---@field rotation number
Transform2D = {}

---@overload fun(position: Vec2|TransformVec2, scale: Vec2|TransformVec2, origin: Vec2|TransformVec2, rotation: number): Transform2D
---@return Transform2D
function Transform2D.new() end

---@overload fun(e: Entity): Transform2D
---@overload fun(e: Entity, position: Vec2|TransformVec2, scale: Vec2|TransformVec2, origin: Vec2|TransformVec2, rotation: number): Transform2D
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

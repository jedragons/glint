---@meta

---@class Sprite
Sprite = {}

---@param atlas string
---@param sprite string
---@return Sprite
function Sprite.load(atlas, sprite) end

---@overload fun(e: Entity, atlas: string, name: string): Sprite
---@param e Entity
---@param sprite Sprite
---@return Sprite
function Sprite.set(e, sprite) end

---@param e Entity
---@return Sprite
function Sprite.get(e) end

---@param e Entity
---@return boolean
function Sprite.has(e) end

---@param e Entity
function Sprite.remove(e) end

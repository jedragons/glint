---@meta

---@class AnimatedSprite
AnimatedSprite = {}

---@param atlas string
---@param name string
---@return AnimatedSprite
function AnimatedSprite.load(atlas, name) end

---@overload fun(e: Entity, atlas: string, name: string): AnimatedSprite
---@param e Entity
---@param animation AnimatedSprite
---@return AnimatedSprite
function AnimatedSprite.set(e, animation) end

---@param e Entity
---@return AnimatedSprite
function AnimatedSprite.get(e) end

---@param e Entity
---@return boolean
function AnimatedSprite.has(e) end

---@param e Entity
function AnimatedSprite.remove(e) end

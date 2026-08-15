---@meta

---@class Atlas
Atlas = {}

---@param path string
---@return Atlas
function Atlas.load(path) end

---@param name string
---@return AnimatedSprite
function Atlas:animation(name) end

---@param name string
---@return Sprite
function Atlas:sprite(name) end

---@meta

---@class Entity
---@field name string
Entity = {}

---@param name string
---@return Entity
function Entity.find(name) end

---@param name string
---@return Entity|nil
function Entity.try_find(name) end

---@return boolean
function Entity:valid() end

function Entity:destroy() end

---@param path string
function Entity:attach_module(path) end

---@meta

---@class Stopwatch
---@field elapsed number
---@field new fun(): Stopwatch
Stopwatch = {}

---@return Stopwatch
function Stopwatch.new() end

---@overload fun(e: Entity): Stopwatch
---@param e Entity
---@param sw Stopwatch
---@return Stopwatch
function Stopwatch.set(e, sw) end

---@param e Entity
---@return Stopwatch
function Stopwatch.get(e) end

---@param e Entity
---@return boolean
function Stopwatch.has(e) end

---@param e Entity
function Stopwatch.remove(e) end

---@return boolean
function Stopwatch:is_paused() end

---@return Stopwatch
function Stopwatch:reset() end

---@overload fun(self: Stopwatch, dt: number): Stopwatch
---@return Stopwatch
function Stopwatch:tick() end

---@return Stopwatch
function Stopwatch:pause() end

---@return Stopwatch
function Stopwatch:unpause() end

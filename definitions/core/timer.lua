---@meta

---@class TimerMode
---@field once integer
---@field repeating integer
TimerMode = {}

---@class Timer
---@field duration number
---@field mode integer
---@field times_finished_this_tick integer
---@field is_finished boolean
---@field just_finished boolean
---@field is_paused boolean
---@field elapsed number
---@field remaining number
---@field fraction number
Timer = {}

---@param duration number
---@param mode integer
---@return Timer
function Timer.new(duration, mode) end

---@overload fun(e: Entity, timer: Timer)
---@param e Entity
---@param duration number
---@param mode integer
---@return Timer
function Timer.set(e, duration, mode) end

---@param e Entity
---@return Timer
function Timer.get(e) end

---@param e Entity
---@return boolean
function Timer.has(e) end

---@param e Entity
function Timer.remove(e) end

function Timer:reset() end

function Timer:pause() end

function Timer:unpause() end

function Timer:finish() end

function Timer:almost_finish() end

---@overload fun(self: Timer, dt: number)
---@return Timer
function Timer:tick() end

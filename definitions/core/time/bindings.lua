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

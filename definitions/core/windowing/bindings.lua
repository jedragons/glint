---@meta

---@class Window
---@field dt number
---@field time number
---@field width integer
---@field height integer
Window = {}

---@param ... any
function log_trace(...) end

---@param ... any
function log_debug(...) end

---@param ... any
function log_info(...) end

---@param ... any
function log_warn(...) end

---@param ... any
function log_error(...) end

---@param ... any
function log_fatal(...) end

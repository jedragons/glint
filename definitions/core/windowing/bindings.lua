---@meta

---@class Window
---@field dt number Read-only.
---@field time number Read-only.
---@field width integer Read-only.
---@field height integer Read-only.
Window = {}

---@type Window
window = nil

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

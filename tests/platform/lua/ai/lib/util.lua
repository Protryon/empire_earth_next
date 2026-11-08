-- A shared library of the runtime test (Data/ai/lib/*.lua in the game).
local M = {}

function M.greet(name)
  return "hello " .. name
end

function M.sum(t)
  local s = 0
  for _, v in ipairs(t) do s = s + v end
  return s
end

return M

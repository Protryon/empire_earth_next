-- The Lua runtime test's personality (tests/platform/lua_test.cpp; ee/EECPLua.h). Data/ai/<name>.lua in the game.
local util = require("lib.util")

local P = {}

function P.init(view, memory)
  memory.steps = 0
  memory.seen = {}
  memory.name = view.name or "none"
  return {hello = util.greet(memory.name)}
end

function P.step(view, memory)
  memory.steps = memory.steps + 1
  memory.seen[#memory.seen + 1] = ai.random(1, 1000)
  if view.mode == "echo" then
    return view.payload
  elseif view.mode == "float" then
    return {build = {count = 1.5}}
  elseif view.mode == "intfloat" then
    return {count = 2.0}
  elseif view.mode == "error" then
    error("step failed on purpose")
  elseif view.mode == "loop" then
    while true do end
  elseif view.mode == "keys" then
    local out = {}
    for k in each(view.payload) do out[#out + 1] = k end
    return out
  elseif view.mode == "badmemory" then
    memory.bad = 0.25
  elseif view.mode == "fixmemory" then
    memory.bad = nil
  end
  return {steps = memory.steps, last = memory.seen[#memory.seen], sum = util.sum(memory.seen)}
end

return P

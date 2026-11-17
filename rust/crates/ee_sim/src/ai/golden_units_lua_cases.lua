local big = {} for i = 1, 5000 do big[i] = i * 7 - 9000 end
local types = {} for i = 1, 20 do types[i] = (i % 3 == 0) and "clubman" or (i % 3 == 1) and i or "nope" end
return {
  5, -3, math.tointeger(2^40), -math.tointeger(2^33), 2.5, 3.0, "x", "clubman", "threat", "lost", "influence",
  "", true, false, {}, {1, 2, 3}, {1, "a", 3.5, 4, 2.0}, big, {1, nil, 3}, {[2] = 5},
  {x = 5, limit = 3, flag = false, name = "n", category = "cat", near = {1, 2, 3}, food = 10, gold = -5, player = 2},
  {x = 2.5, flag = 0, name = 5, near = {1, 2}, food = 2.0, gold = math.tointeger(2^40)},
  {near = {1.5, 2}}, {near = "x"}, {near = {1, 2, 3.5}}, {near = {math.tointeger(2^35), -4, 9}},
  {type = "clubman"}, {type = "citizen"}, {type = "zero"}, {type = types}, {type = 7}, {type = -4}, {type = "nope"},
  {type = 2.5}, {type = {}}, {type = {"nope"}}, {type = {2.5, true, "citizen"}},
  {idle = false, built = true, armed = 0, enemy = nil, held = "h", action = "a", kind = "wood"},
  {idle = true, enemy = false, kind = "silver"}, {kind = 5}, {kind = "iron", player = 2.5, limit = -1},
  {held = {}, action = 3, category = true},
  setmetatable({}, {__index = function(t, k) return 7 end}),
  setmetatable({}, {__index = function(t, k) error("index " .. k) end}),
  setmetatable({}, {__index = function(t, k) if k == "near" then return {4, 5, 6} elseif k == "type" then return {"clubman"} end end}),
  setmetatable({1, 2}, {__index = function(t, k) return "s" .. tostring(k) end}),
  string.rep("q", 300),
}

-- Army helpers over the next engine's view (docs/AI_LUA_PLAN.md §5 "S4 as built": the group verbs and view.groups).
-- require("lib.army"). Generic: no personality's numbers.
local V = require("lib.view")
local M = {}

-- a group of view.groups by id (nil: gone, its units all lost or disbanded)
function M.group(view, id)
    if id == nil then
        return nil
    end
    for _, g in ipairs(view.groups) do
        if g.id == id then
            return g
        end
    end
    return nil
end

-- an entry of view.enemies by player id
function M.enemy(view, player)
    for _, e in ipairs(view.enemies) do
        if e.player == player then
            return e
        end
    end
    return nil
end

-- Group ids come back in the next step's results (view.results, index for index with the orders). M.track(memory,
-- orders, role) remembers which order made the group of a role ("main", "raid", "scout", "home"); M.collect(view,
-- memory) then stores memory.groups[role] = id, and drops the roles whose group is gone.
function M.track(memory, orders, role)
    memory.pending = memory.pending or {}
    memory.pending[role] = #orders
end

function M.collect(view, memory)
    memory.groups = memory.groups or {}
    for _, role in ipairs(ai.keys(memory.pending or {})) do
        local r = view.results[memory.pending[role]]
        if r ~= nil and r.code == "done" and r.group ~= nil then
            memory.groups[role] = r.group
        end
    end
    memory.pending = {}
    for _, role in ipairs(ai.keys(memory.groups)) do
        if M.group(view, memory.groups[role]) == nil then
            memory.groups[role] = nil
        end
    end
end

-- the group of a role this step (nil: none)
function M.role(view, memory, role)
    return M.group(view, (memory.groups or {})[role])
end

-- the military classes a producer (a building id) makes now, the dearest first (the later lines cost more)
function M.classes_of(view, producer)
    local list = {}
    for _, c in ipairs(view.costs) do
        if c.military and not c.building and c.producer == producer then
            list[#list + 1] = c
        end
    end
    table.sort(list, function(a, b)
        local sa, sb = V.sum(a.cost), V.sum(b.cost)
        if sa ~= sb then
            return sa > sb
        end
        return a.type < b.type
    end)
    return list
end

-- whether the stock pays a cost (a per-resource map) n times over the reserve
function M.affords(view, cost, n)
    n = n or 1
    for _, r in ipairs(V.resources) do
        if view.stock[r] - view.reserve[r] < cost[r] * n then
            return false
        end
    end
    return true
end

-- soldiers waiting or in production (all military classes)
function M.coming(view)
    local n = 0
    for _, c in ipairs(view.costs) do
        if c.military and not c.building then
            n = n + c.requested + c.in_production
        end
    end
    return n
end

-- the enemy to fight: the one with the largest economy seen (memory.econ[player], the most citizens and boats seen
-- at once), a known town first; nil when none is known
function M.pick_enemy(view, memory)
    memory.econ = memory.econ or {}
    for _, e in ipairs(view.enemies) do
        local key = tostring(e.player)
        if (memory.econ[key] or 0) < e.citizens_seen then
            memory.econ[key] = e.citizens_seen
        end
    end
    local best, best_score = nil, -1
    for _, e in ipairs(view.enemies) do
        if e.town_x >= 0 or e.citizen_x >= 0 then
            local score = (memory.econ[tostring(e.player)] or 0) * 10 + e.epoch * 100 + (e.town_x >= 0 and 1 or 0)
            if score > best_score then
                best, best_score = e, score
            end
        end
    end
    return best
end

-- the seen enemy strength near the town (the view's threats)
function M.threat(view)
    return view.threat_strength
end

-- the strength of the groups of a task (nil: every group)
function M.strength(view, task)
    local s, n = 0, 0
    for _, g in ipairs(view.groups) do
        if task == nil or g.task == task then
            s = s + g.strength
            n = n + g.units
        end
    end
    return s, n
end

return M

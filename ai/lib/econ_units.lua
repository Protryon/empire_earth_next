-- The economy on the per-unit interface (docs/AI_LUA_UNITS.md §3, §11): the parts of a boom's economy that order
-- single citizens. require("lib.econ_units"). Each function is called from the strategy step and keeps its state in
-- a table of the script's memory (mem below); every one prints its own trace lines.
local V = require("lib.view")

local M = {}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- ---------------------------------------------------------------------------------------------------------------
-- A second town centre (balanced's economy.early_town_centers: 2, docs/AI_RESEARCH.md §8.29; STRATEGY_RESEARCH §4.2
-- item 5: "upgrade a settlement to a town centre for a second citizen producer"). A settlement filled with citizens
-- (its garrison capacity, 5) becomes a town centre; the citizens are used up. Before the Bronze Age only up to `want`
-- town centres and capitols (the owner: "one or two town centres are okay"; "upgrading a settlement to a capitol
-- early game is mostly a waste of food").

local TC_TYPES = {"b_town_center", "b_capitol"}
local TC_EVERY = 20000      -- a new fill no sooner than this after the last (the citizens walk there meanwhile)
local TC_REACH = 40         -- tiles: the citizens sent come from this near the settlement
local TC_SAFE = 12          -- tiles: no seen enemy soldier this near the settlement
local TC_WALK = 60000       -- the citizens sent are held this long at most

-- opts: want (town centres wanted), min_citizens (the town at least this large), safe (no threat at home)
function M.town_centers(view, mem, opts)
    local want = opts.want or 0
    if want <= 1 or view.epoch >= 3 or view.town_x < 0 then   -- (3: the Bronze Age)
        return
    end
    -- (the view's counts: ai.units with a list of types found none here)
    local tcs = 0
    for _, t in ipairs(TC_TYPES) do
        local b = V.buildings(view, t)
        if b ~= nil then
            tcs = tcs + b.finished + b.unfinished
        end
    end
    mem.tcs = tcs
    if mem.tc_walkers ~= nil and view.time >= (mem.tc_walk_until or 0) then
        local left = {}
        for _, id in ipairs(mem.tc_walkers) do
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil then
                left[#left + 1] = id
            end
        end
        if #left > 0 then
            ai.release(left)
        end
        mem.tc_walkers = nil
    end
    if tcs >= want or view.citizens < (opts.min_citizens or 0) or opts.safe == false or mem.tc_walkers ~= nil
            or view.time - (mem.tc_sent or -TC_EVERY) < TC_EVERY then
        return
    end
    -- the finished settlement nearest the town with room, no enemy soldier near and no danger on it
    local best, best_d = nil, nil
    for _, id in ipairs(ai.units{type = "b_settlement", built = true, near = {view.town_x, view.town_y, 60}}) do
        local u = ai.unit(id)
        if u ~= nil and (u.garrisoned or 0) < 5 and ai.danger(u.x, u.y) == 0
                and #ai.seen{enemy = true, armed = true, near = {u.x, u.y, TC_SAFE}, limit = 1} == 0 then
            local d = d2(u.x, u.y, view.town_x, view.town_y)
            if best == nil or d < best_d then
                best, best_d = u, d
            end
        end
    end
    if best == nil then
        return
    end
    local need = 5 - (best.garrisoned or 0)
    -- the citizens nearest it that are not building and not the script's (idle ones first)
    local idle, busy = {}, {}
    for _, id in ipairs(ai.units{category = "citizen", near = {best.x, best.y, TC_REACH}}) do
        local u = ai.unit(id)
        if u ~= nil and u.held ~= "lua" and u.inside == nil and u.action ~= "build" and u.action ~= "repair" then
            if u.action == "idle" then
                idle[#idle + 1] = id
            else
                busy[#busy + 1] = id
            end
        end
    end
    local picks = {}
    for _, list in ipairs({idle, busy}) do
        for _, id in ipairs(list) do
            if #picks < need then
                picks[#picks + 1] = id
            end
        end
    end
    if #picks < need then
        return
    end
    local code, reason, n = ai.garrison(picks, best.id)
    mem.tc_sent = view.time
    print(string.format("econ.tc fill at=%d,%d inside=%d n=%d code=%s reason=%s tcs=%d cit=%d", best.x, best.y,
        best.garrisoned or 0, n or 0, tostring(code), tostring(reason), tcs, view.citizens))
    if code == "ok" then
        -- held while they walk there (a release would hand them to the gatherer runner, which re-targets idle-listed
        -- citizens); inside they are used up. Any still out after TC_WALK is released.
        mem.tc_walkers = picks
        mem.tc_walk_until = view.time + TC_WALK
    end
end

return M

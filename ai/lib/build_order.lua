-- The owner's build order for boom (2026-11-11, docs/AI_RESEARCH.md §8.51; "if it does any more or less than this,
-- it's a failure"): the phases, each phase's exact gatherer targets and buildings, when the next epoch is saved for,
-- and the phase signal the army reads (memory.phase; the raid answers in memory.raid.state, lib.raid).
--
--   pre              12 on the forage patches, settlements by them at once; 3-6 hunting, 5-8 on wood, then 2 each on
--                    iron, gold and stone; 1 barracks, 2-4 towers; no army; 400-500 wood banked at the advance
--   stone_start      a dock and 10 fishing boats (on top of the food citizens), a range; 5-10 soldiers (the army's)
--   stone_grow       the citizens to 12-18 iron, 12-18 gold, 6 stone, 10-15 wood, food unchanged; iron and gold
--                    only near home
--   stone_developed  (latched) the raid of ~20 goes out, ~20 more train at home, then the Copper Age is saved for
--   copper           3 granaries, farmed by the patch workers and hunters and a few more; a stable economy
--
-- The plan of the epoch (boom's PLAN) is overlaid with PHASE[phase]; the soldier fields of a phase belong to the
-- army (boom-defence), the rest to the economy.
local M = {}

local RESOURCES = {"food", "wood", "stone", "gold", "iron"}

-- the gatherers, filled in this order as the citizens come (each step: a resource up to a count); food counts the
-- land gatherers only (the boats come on top)
M.FILL = {
    pre = {{"food", 12}, {"wood", 2}, {"stone", 2}, {"gold", 2}, {"iron", 2}, {"food", 16}, {"wood", 6}, {"food", 18},
           {"wood", 8}},
    stone_start = {{"food", 18}, {"wood", 12}, {"stone", 2}, {"gold", 2}, {"iron", 2}},
    stone_grow = {{"food", 18}, {"wood", 10}, {"stone", 2}, {"gold", 2}, {"iron", 2}, {"iron", 12}, {"gold", 12},
                  {"stone", 6}, {"iron", 15}, {"gold", 15}, {"wood", 12}, {"iron", 18}, {"gold", 18}, {"wood", 15}},
    copper = {{"food", 18}, {"wood", 15}, {"stone", 6}, {"gold", 18}, {"iron", 18}, {"food", 28}, {"wood", 20}},
}
M.FILL.stone_developed = M.FILL.stone_grow

-- the acceptance ranges of each phase (tools: the trace check, econ\order_check.py): gatherers [lo, hi]
M.RANGES = {
    pre = {food = {15, 21}, wood = {5, 8}, stone = {2, 2}, gold = {2, 2}, iron = {2, 2}},
    stone_developed = {food = {15, 21}, wood = {10, 15}, stone = {6, 6}, gold = {12, 18}, iron = {12, 18}},
}

-- the plan overlays (the economy's keys; the army adds its soldier fields here)
M.PHASE = {
    -- (patches 3: the start's 12 on two, the food past the game near home on a third; settlements 7: the patches,
    -- the forest edge and the three mines)
    pre = {citizens = 32, citizens_max = 32, patches = 3, hunt = 6, towers = 3, barracks = 1, ranges = 0,
           barracks_at = 24, settlements = 7},
    -- (from the Stone Age a second town centre: one trains about 2 citizens a minute, the owner's town grew by 3.4 a
    -- minute, 35 to 86 in 15 minutes)
    stone_start = {citizens = 34, citizens_max = 34, boats = 10, docks = 1, barracks = 1, ranges = 1,
                   army_first = false, towers = 4, town_centers = 2, tc_at = 30,
                   soldiers = 8, soldiers_max = 50, army_per = 0, army_floor = 8},   -- (the army's: rs0, boom-defence)
    stone_grow = {citizens = 70, citizens_max = 70, boats = 10, docks = 1, barracks = 1, ranges = 1,
                  army_first = false, towers = 7, town_centers = 2, tc_at = 30,
                  soldiers = 8, soldiers_max = 50, army_per = 0, army_floor = 8},
    stone_developed = {citizens = 70, citizens_max = 70, boats = 10, docks = 1, barracks = 1, ranges = 1,
                       army_first = false, towers = 7, town_centers = 2, tc_at = 30,
                  soldiers = 8, soldiers_max = 50, army_per = 0, army_floor = 8},
    copper = {citizens = 90, citizens_max = 100, boats = 10, docks = 1, granaries = 3, save_at = 75,
              save_after = 12},
}

M.START_MAX_MIN = 6        -- minutes: stone_start ends by then even without the boats (no water, no dock site)
M.GROW_MAX_MIN = 20        -- minutes into the Stone Age: developed by then whatever the gatherers
M.RAID_WAIT_MIN = 15       -- minutes developed: the Copper Age is saved for even if the raid never went out
M.WOOD_BANK = 500          -- wood kept in the Prehistoric reserve from BANK_AT citizens (the dock 175, ten boats 500)
M.BANK_AT = 26

local V = require("lib.view")

local function count(view, type, field)
    local b = V.buildings(view, type)
    if b == nil then
        return 0
    end
    return field == "total" and V.total(b) or b[field] or 0
end
M.count = count

-- the phase of this step (latched forward in memory.phase, its start in memory.phase_since)
function M.phase(view, memory)
    local old = memory.phase
    local p
    if view.epoch == 0 then
        p = "pre"
    elseif view.epoch >= 2 then
        p = "copper"
    else
        local in_epoch = (view.time - view.epoch_start) // 60000
        p = old
        if p == nil or p == "pre" or p == "copper" then
            p = "stone_start"
        end
        if p == "stone_start" and ((count(view, "b_dock", "finished") >= 1 and view.boats >= 10)
                or in_epoch >= M.START_MAX_MIN) then
            p = "stone_grow"
        end
        if p == "stone_grow" then
            local g = view.gatherers
            if ((g.iron or 0) >= 12 and (g.gold or 0) >= 12 and (g.stone or 0) >= 6 and (g.wood or 0) >= 10)
                    or in_epoch >= M.GROW_MAX_MIN then
                p = "stone_developed"
            end
        end
    end
    if p ~= old then
        memory.phase_since = view.time
        print(string.format("order phase=%s from=%s t=%d", p, tostring(old), view.time // 1000))
    end
    memory.phase = p
    return p
end

-- the epoch's plan overlaid with the phase's (a copy)
function M.plan(plan, phase)
    local p = {}
    for k, v in pairs(plan) do
        p[k] = v
    end
    for k, v in pairs(M.PHASE[phase] or {}) do
        p[k] = v
    end
    -- (the spill and the citizens' work, boom: each resource's most is the fill's)
    local cap = {}
    for _, r in ipairs(RESOURCES) do
        cap[r] = 0
    end
    for _, s in ipairs(M.FILL[phase] or {}) do
        cap[s[1]] = math.max(cap[s[1]], s[2])
    end
    cap.food = nil
    p.cap = cap
    p.floor = {wood = 0, stone = 0, gold = 0, iron = 0}
    return p
end

-- the gatherers wanted of `land` citizens: the phase's fill in order, each resource within its `room` (the places it
-- has: the food places, the claimed mines' places; nil: no limit). A step a resource has no room for goes on to the
-- next steps; what the fill leaves goes to food, then wood, within their room (none is left over when the citizens
-- are capped at M.capacity)
function M.fill(phase, land, room, keep_left)
    room = room or {}
    local want = {food = 0, wood = 0, stone = 0, gold = 0, iron = 0}
    local left = land
    local function give(r, upto)
        local most = math.min(upto, room[r] or upto)
        local n = math.max(math.min(most - want[r], left), 0)
        want[r] = want[r] + n
        left = left - n
    end
    for _, s in ipairs(M.FILL[phase] or {}) do
        give(s[1], s[2])
    end
    if keep_left then
        return want, left
    end
    give("food", want.food + left)
    -- (no room anywhere: on wood up to the phase's most, then wherever the engine sets them; the citizens are capped
    -- at M.capacity, so this is only the town that lost a site)
    give("wood", math.max(want.wood, M.most(phase, "wood")))
    return want, left
end

-- the citizens the phase's fill has work for within `room`
function M.capacity(phase, room)
    local _, left = M.fill(phase, 1000, room, true)
    return 1000 - left
end

-- the most the fill puts on a resource
function M.most(phase, resource)
    local n = 0
    for _, s in ipairs(M.FILL[phase] or {}) do
        if s[1] == resource then
            n = math.max(n, s[2])
        end
    end
    return n
end

-- whether the next epoch is saved for (`default`: the plan's save_at / save_after rule)
function M.saving(view, memory, phase, default)
    if phase == "pre" or phase == "copper" then
        return default
    end
    if phase ~= "stone_developed" then
        return false
    end
    local raid = memory.raid ~= nil and memory.raid.state or nil
    if raid == nil or raid == "sent" or raid == "done" then
        return true
    end
    return view.time - (memory.phase_since or view.time) >= M.RAID_WAIT_MIN * 60000
end

-- the buildings that wait while the boats come (the bank pays for the dock and the boats first)
function M.holds_wood(view, phase)
    return phase == "stone_start" and view.boats < 10
end

-- the trace line (once a minute): what the acceptance check reads
function M.trace(view, memory, phase, army)
    local g = view.gatherers
    print(string.format("order line phase=%s gat=%d/%d/%d/%d/%d hunters=%d boats=%d cit=%d army=%d docks=%d barracks=%d "
        .. "ranges=%d towers=%d settlements=%d granaries=%d farms=%d raid=%s", phase, g.food or 0, g.wood or 0, g.stone or 0,
        g.gold or 0, g.iron or 0, view.hunters or 0, view.boats, view.citizens, army, count(view, "b_dock", "finished"),
        count(view, "b_barracks", "finished"), count(view, "b_archery_range", "finished"), M.towers(view),
        count(view, "b_settlement", "finished"), count(view, "b_granary", "finished"), count(view, "b_farm", "finished"),
        memory.raid ~= nil and tostring(memory.raid.state) or "-"))
end

local TOWERS = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle"}
function M.towers(view)
    local n = 0
    for _, t in ipairs(TOWERS) do
        n = n + count(view, t, "finished")
    end
    return n
end

return M

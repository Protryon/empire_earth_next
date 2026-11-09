-- The rush personality (docs/AI_LUA_PLAN.md §4 and §5 "S4 as built").
-- An early army from two barracks with no Prehistoric cap, a scout to find the target, raids on its gatherers and
-- fishing boats away from its towers, waves at its town (one at once when it reaches a new epoch), production all the
-- way. When the rush fails (two waves lost, or the target walled in towers and an army), it falls back to a boom: more
-- citizens, towers, docks, the epochs, and a home guard that attacks again only from strength.
local V = require("lib.view")
local E = require("lib.rb_economy")
local R = require("lib.rb_army")

-- the rush: a small, iron-heavy town; Stone after the first waves
local RUSH = {
    citizens = {30, 60, 80},
    shares = {
        {wood = 14, stone = 0, gold = 4, iron = 30},
        {wood = 18, stone = 4, gold = 12, iron = 26},
        {wood = 18, stone = 6, gold = 14, iron = 22},
    },
    least = {{wood = 3, iron = 5}, {wood = 5, iron = 6, gold = 3}, {wood = 6, iron = 6, gold = 4}},
    most = {{gold = 2, stone = 0, iron = 9}, {gold = 9, iron = 14, wood = 12, stone = 3}, {gold = 12, iron = 18, wood = 16, stone = 5}},
    food_least = 40,
    towers = {0, 1, 2},
    tower_citizens = 20,
    settlements = 1,
    boats = {0, 4, 6},
    docks = {0, 1, 1},
    buildings = {
        {{type = "b_barracks", n = 1, citizens = 7}, {type = "b_barracks", n = 2, citizens = 11}},
        {{type = "b_barracks", n = 2}, {type = "b_archery_range", n = 1, citizens = 20}},
        {{type = "b_barracks", n = 2}, {type = "b_archery_range", n = 1}, {type = "b_stable", n = 1},
         {type = "b_farm", n = 8}},
    },
    save_slack = 0,
    save_after = {12 * 60000, 16 * 60000},
}

-- the fallback boom (the balanced economy, a little smaller)
local BOOM = {
    citizens = {30, 80, 95},
    shares = {
        {wood = 15, stone = 3, gold = 8, iron = 18},
        {wood = 18, stone = 6, gold = 12, iron = 16},
        {wood = 18, stone = 8, gold = 12, iron = 16},
    },
    least = {{wood = 3, iron = 4, gold = 2}, {wood = 8, iron = 6, gold = 4, stone = 2}, {wood = 10, iron = 8, gold = 6, stone = 3}},
    most = {{gold = 3, iron = 6, stone = 2}, {gold = 12, iron = 18, wood = 20, stone = 6}, {gold = 14, iron = 20, wood = 23, stone = 8}},
    food_least = 45,
    towers = {2, 4, 6},
    tower_citizens = 14,
    settlements = 2,
    boats = {0, 8, 10},
    docks = {0, 2, 2},
    buildings = {
        {{type = "b_barracks", n = 2}},
        {{type = "b_barracks", n = 2}, {type = "b_archery_range", n = 1}},
        {{type = "b_barracks", n = 2}, {type = "b_archery_range", n = 1}, {type = "b_stable", n = 1},
         {type = "b_farm", n = 12}},
    },
    save_slack = 4,
    save_after = {16 * 60000, 20 * 60000},
}

local PRODUCERS = {{"b_barracks"}, {"b_barracks", "b_archery_range"}, {"b_barracks", "b_archery_range", "b_stable"}}

local RUSH_ARMY = {
    producers = PRODUCERS,
    queue = 2,
    -- no cap but the population: the rush keeps training (the economy keeps the citizens coming first)
    army = function(view, memory)
        -- (but a town well short of its citizens holds the army at what it has, 8 at least)
        if view.citizens < RUSH.citizens[E.age(view)] - 4 and view.army_count >= 8 then
            return view.army_count
        end
        return 200
    end,
    scout_at = 60000,
    rescout_ms = 300000,
    -- a wave: 6 soldiers in the Prehistoric, 10 later; more when the target was seen with more
    wave = function(view, memory, e)
        local base = view.epoch == 0 and 10 or 14
        local seen = e.seen_army + e.towers * 2
        return math.max(base, seen + 2)
    end,
    raid_size = 4,
    raid_every = 180000,
    raid_ok = function(view, memory, e)
        return true
    end,
    retreat_ratio = 180,
    recall_ratio = 200,
    punish_epoch = true,
}

local BOOM_ARMY = {
    producers = PRODUCERS,
    queue = 1,
    home_keep = 4,
    army = function(view, memory)
        local base = ({6, 14, 20})[E.age(view)]
        local strongest = 0
        for _, e in ipairs(view.enemies) do
            if e.last_seen > 0 and view.time - e.last_seen < 600000 then
                strongest = math.max(strongest, e.seen_army)
            end
        end
        return math.max(base, strongest + 2)
    end,
    scout_at = 60000,
    rescout_ms = 600000,
    -- again only from strength: half as strong again as what was seen, and a real army
    wave = function(view, memory, e)
        if view.army_strength * 10 < e.seen_strength * 16 or view.army_count < 14 then
            return nil
        end
        return 14
    end,
    raid_size = 4,
    raid_every = 480000,
    raid_ok = function(view, memory, e)
        return view.threat_strength == 0 and e.towers <= 2
    end,
    retreat_ratio = 130,
    recall_ratio = 120,
    punish_epoch = false,
}

local M = {}

function M.step(view, memory)
    local orders = {}
    if view.town_x < 0 then
        return orders
    end
    memory.mode = memory.mode or "rush"
    memory.army = memory.army or {}
    -- the rush has failed: two waves lost, or (from 25 minutes) a target with towers and as large an army; or it is
    -- still in the Prehistoric at 30 minutes
    if memory.mode == "rush" then
        local failed = memory.army.failed or 0
        local late = view.time >= 25 * 60000
        local walled = false
        for _, e in ipairs(view.enemies) do
            if e.towers >= 4 and e.seen_strength > view.army_strength then
                walled = true
            end
        end
        if failed >= 2 or (late and walled) or (view.epoch == 0 and view.time >= 30 * 60000) then
            memory.mode = "boom"
            memory.boom_at = view.time
            print("rush: falls back to a boom at " .. tostring(view.time // 60000) .. " min (waves lost " .. tostring(failed) .. ")")
        end
    end
    local plan = memory.mode == "rush" and RUSH or BOOM
    local army = memory.mode == "rush" and RUSH_ARMY or BOOM_ARMY
    -- under threat the boom keeps a tower more and holds the epoch's reserve back
    local opts = nil
    if memory.mode == "boom" and view.threat_strength > view.army_strength then
        opts = {towers = (plan.towers[E.age(view)] or 0) + 1}
    end
    opts = opts or {}
    opts.reserve_less = R.floor_cost(view, memory, army)
    local saving, short = E.step(view, memory, plan, orders, opts)
    R.step(view, memory, army, orders, {saving = saving, short = short})
    return orders
end

return M

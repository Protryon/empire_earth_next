-- The balanced personality (docs/AI_LUA_PLAN.md §4 and §5 "S4 as built"): a boom-like economy on the owner's numbers
-- (food first, 85-90 citizens in the Stone Age, gold and iron gatherers capped, few production buildings, towers), a
-- mid-size army that grows with what its scout sees, a home guard that defends the town, attacks when it is clearly
-- stronger, raids when the enemy's gatherers stand away from towers, and retreats from a fight it is losing.
local V = require("lib.view")
local E = require("lib.rb_economy")
local R = require("lib.rb_army")

local PLAN = {
    citizens = {32, 88, 100},
    shares = {
        {wood = 15, stone = 3, gold = 8, iron = 18},
        {wood = 18, stone = 6, gold = 12, iron = 16},
        {wood = 18, stone = 8, gold = 12, iron = 16},
    },
    least = {{wood = 3, iron = 4, gold = 2}, {wood = 8, iron = 6, gold = 4, stone = 2}, {wood = 10, iron = 8, gold = 6, stone = 3}},
    most = {{gold = 3, iron = 6, stone = 2}, {gold = 12, iron = 18, wood = 23, stone = 6}, {gold = 14, iron = 20, wood = 23, stone = 8}},
    food_least = 45,
    towers = {2, 4, 6},
    tower_citizens = 14,
    settlements = 2,
    boats = {0, 10, 12},
    docks = {0, 2, 2},
    buildings = {
        {{type = "b_barracks", n = 1, citizens = 12}},
        {{type = "b_barracks", n = 1}, {type = "b_archery_range", n = 1, citizens = 40}},
        {{type = "b_barracks", n = 2}, {type = "b_archery_range", n = 1}, {type = "b_stable", n = 1},
         {type = "b_farm", n = 12}},
    },
    save_slack = 4,
    save_after = {13 * 60000, 18 * 60000},
}

local PRODUCERS = {{"b_barracks"}, {"b_barracks", "b_archery_range"}, {"b_barracks", "b_archery_range", "b_stable"}}

-- the strongest enemy army seen in the last 10 minutes
local function strongest(view)
    local count, strength = 0, 0
    for _, e in ipairs(view.enemies) do
        if e.last_seen > 0 and view.time - e.last_seen < 600000 then
            count = math.max(count, e.seen_army)
            strength = math.max(strength, e.seen_strength)
        end
    end
    return count, strength
end

local ARMY = {
    producers = PRODUCERS,
    queue = 2,
    home_keep = 4,
    -- a mid-size army: 6, 16, 26 by age; half as large again as the most the scout saw, at most a third of the population
    army = function(view, memory)
        local base = ({6, 16, 26})[E.age(view)]
        local count = strongest(view)
        local want = math.max(base, count + count // 2 + 2)
        return math.min(want, view.population_cap // 3)
    end,
    scout_at = 150000,
    rescout_ms = 480000,
    -- attack when clearly stronger than the target's army seen, with a real army
    wave = function(view, memory, e)
        local need = ({8, 14, 20})[E.age(view)]
        if view.army_count < need or view.threat_strength > 0 then
            return nil
        end
        if view.army_strength * 10 < (e.seen_strength + e.towers * 3000) * 15 then
            return nil
        end
        return need
    end,
    raid_size = 4,
    raid_every = 360000,
    raid_ok = function(view, memory, e)
        return view.threat_strength == 0 and view.army_count >= 8 and e.towers <= 3
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
    -- threatened: a tower more
    local opts = nil
    if view.threat_strength > view.army_strength then
        opts = {towers = (PLAN.towers[E.age(view)] or 0) + 1}
    end
    local saving, short = E.step(view, memory, PLAN, orders, opts)
    R.step(view, memory, ARMY, orders, {saving = saving, short = short})
    return orders
end

return M

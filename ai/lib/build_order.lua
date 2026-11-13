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

-- the parts added after O3 (AI_RESEARCH §8.51), each switchable for the bisect of O8's slower Copper Age
M.OPT = {
    place = true,           -- settlements touching their resource (lib.place), home sites included
    techs = true,           -- the town centre's gathering techs
    stone_uncap = true,     -- no citizen cap at the places' work in the Stone Age
    claim_target = true,    -- the mines claimed for the phase's whole target
    food_reach = false,     -- the Stone claim reaches claim_dry while the food places are short (off: the owner's playtest,
                            -- "unpopulated food settlement near my base"; the bisect: no gain)
    boat_wood = false,      -- (on in boom_order, §8.51) in stone_start, until the boats are out, the settlements wait
                            -- too: the bank's wood pays the dock and the boats first
    shore_scout = false,    -- (off until benched) one wood gatherer walks the shore round home before the advance
    lua_farms = false,      -- (on in boom_order, §8.51) the Copper Age's 3 granaries sited by lib.place one after
                            -- another, and the engine builds no granary's fields (gather granary_farms = 0) until all
                            -- stand, then 8 each
    soldiers_after_boats = false, -- (on in boom_order) stone_start trains no soldier before 10 boats, unless threatened
    settle_afford = false,  -- (off until benched) a settlement is placed only with its whole cost in hand, and one refused
                            -- for its cost tries again in 20 s, not 5 minutes
    builder_pool = false,   -- (off until benched) the Stone Age's citizens: the fill's places plus those building (at most
                            -- BUILDER_POOL), never past STONE_MOST; none for a resource past its most
    blocked_skip = false,   -- (on in boom_order) a foundation lost on a blocked tile passes over every tile it covered
    mines_early = false,    -- (off until benched) from the advance the gold and iron mines are claimed for stone_grow's
                            -- target and their settlements pass the boat hold once the dock stands; lib.place retries a
                            -- cost refusal in 20 s
    boats_then_mines = false, -- (off until benched) past 10 boats, no more in the Stone Age while a kept mine waits for
                            -- its settlement
    owner_mines = false,    -- (off until benched) the owner's mines (his answers, 2026-11-13): the second gold and iron
                            -- mines claimed and settled in the Prehistoric (touching, no food gatherers building); a
                            -- third mine of a resource only past 12 on it; wood 7 until Stone +12; the bank 575
    pre_mines = false,      -- (off until benched) the Prehistoric part of owner_mines alone
    pre_lean = false,       -- (off until benched) the owner's Prehistoric settlements: two patches while two home patches
                            -- stand, one settlement a site (any kind touched), none for wood
    mines_after_barracks = false, -- (off until benched) a Prehistoric mine settlement waits for the barracks and a tower
    owner_barracks = false, -- (off until benched) the Prehistoric barracks at OWNER_BARRACKS_AT citizens (his: 2.4 min)
    pre_dock = false,       -- the dock built while the Stone Age is saved for (off: the fish verb refuses a Prehistoric
                            -- dock, not_in_epoch)
}

-- the gatherers, filled in this order as the citizens come (each step: a resource up to a count); food counts the
-- land gatherers only (the boats come on top)
M.FILL = {
    pre = {{"food", 12}, {"wood", 2}, {"stone", 2}, {"gold", 2}, {"iron", 2}, {"food", 16}, {"wood", 6}, {"food", 18},
           {"wood", 8}},
    stone_start = {{"food", 18}, {"wood", 12}, {"stone", 2}, {"gold", 2}, {"iron", 2}},
    stone_grow = {{"food", 18}, {"wood", 10}, {"stone", 2}, {"gold", 2}, {"iron", 2}, {"iron", 12}, {"gold", 12},
                  {"stone", 6}, {"iron", 15}, {"gold", 15}, {"wood", 12}, {"iron", 18}, {"gold", 18}, {"wood", 15}},
    -- (the owner's Copper town: 120-133 citizens, food 25-44 with 13 boats, wood 16-42; "not enough cits" at 80 min)
    copper = {{"food", 18}, {"wood", 15}, {"stone", 6}, {"gold", 18}, {"iron", 18}, {"food", 28}, {"wood", 20},
              {"food", 40}, {"wood", 30}},
}
M.FILL.stone_developed = M.FILL.stone_grow
-- (OPT.owner_mines: the citizens the early Stone wood leaves go to the Prehistoric's two settled mines, 6 each)
M.FILL_OWNER = {
    stone_start = {{"food", 18}, {"wood", 12}, {"stone", 2}, {"gold", 2}, {"iron", 2}, {"iron", 6}, {"gold", 6}},
}

-- the acceptance ranges of each phase (tools: the trace check, econ\order_check.py): gatherers [lo, hi]
M.RANGES = {
    pre = {food = {15, 21}, wood = {5, 8}, stone = {2, 2}, gold = {2, 2}, iron = {2, 2}},
    stone_developed = {food = {15, 21}, wood = {10, 15}, stone = {6, 6}, gold = {12, 18}, iron = {12, 18}},
}

-- the phase's fill (OPT.owner_mines: its own where it has one)
local function fill_of(phase)
    return (M.OPT.owner_mines and M.FILL_OWNER[phase]) or M.FILL[phase] or {}
end

-- the plan overlays (the economy's keys; the army adds its soldier fields here)
M.PHASE = {
    -- (patches 3: the start's 12 on two, the food past the game near home on a third; settlements 10: the patches,
    -- the forest edge, the three mines, and the touching ones by the home patches and mines the town centre served:
    -- at 7 the third patch stood claimed with no settlement in every game, food 15.9 on 18 wanted)
    pre = {citizens = 32, citizens_max = 32, patches = 3, hunt = 6, towers = 3, barracks = 1, ranges = 0,
           barracks_at = 24, settlements = 10},
    -- (from the Stone Age a second town centre: one trains about 2 citizens a minute, the owner's town grew by 3.4 a
    -- minute, 35 to 86 in 15 minutes)
    -- (one town centre until the boats and the soldiers: the second's 5 citizens garrisoned at the advance left the town
    -- at its weakest, boom-defence's seeds 4 and 24)
    stone_start = {citizens = 76, citizens_max = 76, boats = 13, docks = 1, barracks = 2, ranges = 1,
                   army_first = false, towers = 4, town_centers = 1, tc_at = 30, claim_dry = 45, settlements = 14,
                   soldiers = 8, soldiers_max = 50, army_per = 0, army_floor = 8},   -- (the army's: rs0, boom-defence)
    stone_grow = {citizens = 76, citizens_max = 76, boats = 13, docks = 1, barracks = 2, ranges = 1,
                  army_first = false, towers = 7, town_centers = 2, tc_at = 30, claim_dry = 45, settlements = 14,
                  soldiers = 8, soldiers_max = 50, army_per = 0, army_floor = 8},
    stone_developed = {citizens = 76, citizens_max = 76, boats = 13, docks = 1, barracks = 2, ranges = 1,
                       army_first = false, towers = 7, town_centers = 2, tc_at = 30, claim_dry = 45, settlements = 14,
                  soldiers = 8, soldiers_max = 50, army_per = 0, army_floor = 8},
    copper = {citizens = 120, citizens_max = 130, boats = 13, docks = 1, granaries = 3, settlements = 16, save_at = 75,
              save_after = 12},
}

M.START_MAX_MIN = 6        -- minutes: stone_start ends by then even without the boats (no water, no dock site)
M.GROW_MAX_MIN = 30        -- minutes into the Stone Age: developed by then whatever the gatherers (a safety only: the owner
                           -- saves once the town is developed, 32 -> 86 citizens in minutes 22-37.5, then Copper at 45)
M.RAID_WAIT_MIN = 15       -- minutes developed: the Copper Age is saved for even if the raid never went out
M.FOOD_BANK = 0            -- food kept past the Stone Age's cost at the advance: the 5-10 soldiers trained at once (8 x 30)
M.WOOD_BANK = 500          -- wood kept in the Prehistoric reserve while the Stone Age is saved for (the dock 175, ten
                           -- boats 500)
M.WOOD_BANK_OWNER = 575    -- (OPT.owner_mines) the owner's bank at the advance (his playtest logs: 575 wood)
M.STONE_WOOD = 7           -- (OPT.owner_mines) wood gatherers at most until STONE_WOOD_MIN into the Stone Age
M.STONE_WOOD_MIN = 12
M.wood_cap = nil           -- (set by boom each step)
M.OWNER_BARRACKS_AT = 6    -- (OPT.owner_barracks) citizens
M.BUILDER_POOL = 11        -- (OPT.builder_pool) the Stone Age's citizens building on top of the fill's places, at most
M.STONE_MOST = 86          -- ... and the Stone Age's citizens at most (the owner's 35 -> 86)
M.BUILDER_IDLE = 4         -- (OPT.builder_pool) no citizen trained while this many stand idle

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
            -- (the targets, or the mines' places all worked: a map with one gold mine near home holds 6, never 12)
            local room = memory.order_room or {}
            local function met(r, n)
                return (g[r] or 0) >= math.min(n, math.max(room[r] or n, 1))
            end
            -- (the owner's targets only: 12+ gold, 12+ iron, 6 stone, 10+ wood, the boats; the timer is a safety)
            if ((g.iron or 0) >= 12 and (g.gold or 0) >= 12 and (g.stone or 0) >= 6 and (g.wood or 0) >= 10
                    and view.boats >= 10) or in_epoch >= M.GROW_MAX_MIN then
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
    for _, s in ipairs(fill_of(phase)) do
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
        if r == "wood" and M.wood_cap ~= nil then   -- (OPT.owner_mines: the early Stone wood, set by boom each step)
            most = math.min(most, M.wood_cap)
        end
        local n = math.max(math.min(most - want[r], left), 0)
        want[r] = want[r] + n
        left = left - n
    end
    for _, s in ipairs(fill_of(phase)) do
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
    for _, s in ipairs(fill_of(phase)) do
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

-- the start patches' food left (the owner: "a failure if the initial patches run out before Copper"): the home forage
-- patches seen at the first step, their amounts now (0 once gone)
function M.start_patches(view, memory)
    if memory.start_patches == nil then
        memory.start_patches = {}
        for _, s in ipairs(view.sites) do
            if s.home and s.patch then
                memory.start_patches[#memory.start_patches + 1] = s.index
            end
        end
    end
    local left, alive = 0, 0
    for _, idx in ipairs(memory.start_patches) do
        for _, s in ipairs(view.sites) do
            if s.index == idx and s.patch and (s.amount or 0) > 0 then
                left = left + s.amount
                alive = alive + 1
            end
        end
    end
    return left, alive, #memory.start_patches
end

-- the trace line (once a minute): what the acceptance check reads
function M.trace(view, memory, phase, army)
    local g = view.gatherers
    local pleft, palive, pn = M.start_patches(view, memory)
    print(string.format("order line phase=%s gat=%d/%d/%d/%d/%d hunters=%d boats=%d cit=%d army=%d docks=%d barracks=%d "
        .. "ranges=%d towers=%d settlements=%d granaries=%d farms=%d raid=%s tplaces=%d/%d/%d startfood=%d/%d/%d", phase, g.food or 0, g.wood or 0, g.stone or 0,
        g.gold or 0, g.iron or 0, view.hunters or 0, view.boats, view.citizens, army, count(view, "b_dock", "finished"),
        count(view, "b_barracks", "finished"), count(view, "b_archery_range", "finished"), M.towers(view),
        count(view, "b_settlement", "finished"), count(view, "b_granary", "finished"), count(view, "b_farm", "finished"),
        memory.raid ~= nil and tostring(memory.raid.state) or "-", (memory.touched_room or {}).stone or 0,
        (memory.touched_room or {}).gold or 0, (memory.touched_room or {}).iron or 0, pleft, palive, pn))
end

-- the town centre's gathering techs (dbtechtree: kind 0, researched at b_town_center or b_capitol; the owner takes them at
-- once, epoch by epoch): {tech, its epoch}. Their events name what they raise (hafts: stone, hand axe and saw: wood,
-- metal working and amalgamation: gold, shaft mining: iron, windlass: stone, land conservation: farms ...)
M.TECHS = {
    {"hafts_tech", 1}, {"hand_axe_tech_from_town_center", 2}, {"fine_flaked_tools", 2}, {"metal_working_tech", 3},
    {"hardened_chisel_tech", 3}, {"falconry_tech", 4}, {"saw_tech", 4}, {"amalgamation_tech", 5},
    {"shaft_mining_tech", 5}, {"windlass_tech", 6}, {"land_conservation_tech", 7}, {"forestry_tech", 7},
    {"steam_engine_tech", 8}, {"mining_explosives_tech", 8}, {"hammer_drilling_rigs_tech", 10},
    {"enviromental_law_tech", 10}, {"mechanized_logging_tech", 10}, {"oxygen_furnace_tech", 11},
    {"hydraulic_mining_tech", 11}, {"self_sufficiency_tech", 12}, {"atom_manipulation_tech", 13},
    {"core_mining_tech", 13},
}

-- the next gathering tech to research now (nil: none due); memory.techs: the ones ordered
function M.next_tech(view, memory)
    memory.techs = memory.techs or {}
    for _, t in ipairs(M.TECHS) do
        if t[2] <= view.epoch and not memory.techs[t[1]] then
            return t[1]
        end
    end
    return nil
end

-- (BO.OPT.shore_scout) the shore by home seen before the advance: a building goes only on tiles in sight, and the
-- dock finder found 9-18 fish within 30 tiles but no shore tile for them on the seeds whose dock came 10-62 minutes
-- into Stone. While the Stone Age is saved for, one wood gatherer walks a ring of 8 points 16 tiles round the town
M.SCOUT_RING = 16
function M.shore_scout(view, memory, saving)
    local S = memory.shore_scout
    if S == nil then
        if not saving or view.epoch ~= 0 then
            return
        end
        local pick = nil
        for _, id in ipairs(ai.units{category = "citizen", held = "engine", near = {view.town_x, view.town_y, 30}, limit = 16}) do
            local u = ai.unit(id)
            if pick == nil and u ~= nil and u.gathers == "wood" then
                pick = id
            end
        end
        if pick == nil then
            return
        end
        ai.take({pick})
        memory.shore_scout = {id = pick, i = 0, t = 0}
        S = memory.shore_scout
    end
    if S.done then
        return
    end
    local u = ai.unit(S.id)
    if u == nil then
        S.done = true
        return
    end
    if view.time - S.t >= 20000 or (u.action ~= "move" and view.time - S.t >= 3000) then
        S.i = S.i + 1
        if S.i > 8 then
            ai.release({S.id})
            S.done = true
            return
        end
        -- (the 8 directions in tenths: no trigonometry in the sandbox)
        local d = ({{10, 0}, {7, 7}, {0, 10}, {-7, 7}, {-10, 0}, {-7, -7}, {0, -10}, {7, -7}})[S.i]
        local x = math.max(view.town_x + M.SCOUT_RING * d[1] // 10, 0)
        local y = math.max(view.town_y + M.SCOUT_RING * d[2] // 10, 0)
        ai.move({S.id}, x, y)
        S.t = view.time
    end
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

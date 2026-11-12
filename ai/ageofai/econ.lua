-- Age of AI port, the economy (upstream dll/ai/econ.c, early.c, food.c, mines.c, citizens.c, defence.c's towers;
-- UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors). require("ageofai.econ").
--
-- Upstream steers the original game's managers: it hides a reserve and a military account from the stockpile,
-- redistributes the resource manager's worker targets by shortfall, and queues the buildings it wants. This port
-- does the same through the next engine's coarse verbs (docs/AI_LUA_PLAN.md §3.2: gather, claim, build, train,
-- research, reserve, advance, fish): the engine's finders and runners carry the orders out, as the game's managers
-- did for upstream. What a verb cannot say stays in Lua (the accounts, the targets, the plan).
local V = require("lib.view")
local S = require("ageofai.settings")
local P = require("ageofai.priority")   -- [priority.c] the list every spender asks
local MT = require("ageofai.metal")     -- [metal.c] workers_to_metals
local PK = require("ageofai.packing")   -- [packing.c] the production buildings packed against their kind
local M = {}

local cur_memory = nil   -- [priority.c] the memory of the step under way (the buildings ask the list)

local FARM_EPOCH = 2                -- the Copper Age: farms make food renewable
local PASS_MS = 10000               -- the worker plan, claims and buildings every 10 s
local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_digital", "b_guard_tower_space"}
local NOT_FOR_SOL = {b_town_center = true, b_capitol = true, b_settlement = true, b_farm = true, b_granary = true,
                     b_house = true, b_dock = true}
-- the gathering upgrades (gather_research: every available one, one every 15 s), oldest first
local GATHER_TECHS = {"hafts_tech", "hand_axe_tech_from_town_center", "fine_flaked_tools", "hardened_chisel_tech",
                      "shaft_mining_tech", "saw_tech", "irrigation", "wheeled_drawn_plow", "mining_explosives_tech",
                      "granary_upgrade", "hydraulic_mining_tech", "core_mining_tech"}
local MINERALS = {"stone", "gold", "iron"}
local SETTLE_RANK = {food = 1, wood = 2, iron = 3, gold = 4, stone = 5}
local SETTLE_AT_ONCE = 2
local SETTLE_WAIT = 60000

local function clamp(v, lo, hi)
    if hi ~= nil and v > hi then
        v = hi
    end
    if lo ~= nil and v < lo then
        v = lo
    end
    return v
end

local function isqrt(v)
    if v <= 0 then
        return 0
    end
    local r = math.floor(math.sqrt(v))
    while r * r > v do
        r = r - 1
    end
    while (r + 1) * (r + 1) <= v do
        r = r + 1
    end
    return r
end

function M.tower_class(view)
    for t = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[t]) ~= nil then
            return TOWER_CLASSES[t]
        end
    end
    return nil
end

function M.tower_count(view)
    local n = 0
    for _, t in ipairs(TOWER_CLASSES) do
        n = n + V.total(V.buildings(view, t))
    end
    return n
end

local function tower_counts(view)
    local n, finished, busy = 0, 0, false
    for _, t in ipairs(TOWER_CLASSES) do
        local b = V.buildings(view, t)
        if b ~= nil then
            n = n + V.total(b)
            finished = finished + b.finished
            busy = busy or b.unfinished + b.queued > 0
        end
    end
    return n, finished, busy
end

-- ---------------------------------------------------------------------------------------------------------------
-- The budget (econ.c budget_reserve, spend_reserve, hook_add_resource)

-- the epoch reserve: before farms the next epoch's cost once the workforce or the epoch's age says so (the copper
-- plan: the economy first, then the saving); from farms the cost grows in over reserve_window_min minutes from
-- the planned attempt (timer pacing) or stands at once (rush)
function M.reserve(view, mem)
    local r = {}
    local saving = false
    -- RECOVERY (econ.c recovering): from the Copper Age, while the workforce is under min_workforce (5 of slack
    -- once on), once it had min_workforce + 5 or from minute 30
    if S.recover_economy and view.epoch >= FARM_EPOCH then
        if view.citizens >= S.min_workforce + 5 then
            mem.had_economy = true
        end
        local on = mem.recovering and view.citizens < S.min_workforce + 5 or view.citizens < S.min_workforce
        if not mem.had_economy and view.time < 1800000 then
            on = false
        end
        if (on or false) ~= (mem.recovering or false) then
            print(string.format("recovery %s workforce=%d min=%d", on and "the economy first" or "over", view.citizens, S.min_workforce))
        end
        mem.recovering = on or nil
    else
        mem.recovering = nil
    end
    local both = false
    if view.epoch < view.last_epoch and not view.advancing then
        if view.epoch < FARM_EPOCH then
            -- COPPER PLAN (copper_plan=on): from min_workforce on the full remaining cost of the Copper Age (both
            -- epochs' costs in the first) is reserved against military spending; below it the citizens ignore the
            -- reserve (the economy first) and no early advance
            saving = view.citizens >= S.min_workforce
            both = view.epoch == 0 and S.copper_plan ~= "next_epoch"   -- next_epoch: only the next epoch's cost
        elseif S.epoch_pacing == "rush" then
            saving = true
        else
            -- the timer (epoch_min..max minutes from the epoch's start); "independent": the game's catch-up when
            -- another player is ahead (EEComputerPlayer::CatchUpEpoch's 1..12 minutes x Hard's 70%) - no human here
            if S.epoch_pacing == "independent" then
                local ahead = false
                for _, en in ipairs(view.enemies or {}) do
                    if (en.epoch or 0) > view.epoch then
                        ahead = true
                    end
                end
                if ahead and (mem.catch_epoch or -1) ~= view.epoch then
                    mem.catch_epoch = view.epoch
                    local at = view.time + (S.catch_up_min + ai.random(0, S.catch_up_max - S.catch_up_min)) * 60000
                    if (mem.epoch_at or 0) > at then
                        mem.epoch_at = at
                        print(string.format("epoch catch_up at=%d", at // 60000))
                    end
                end
            end
            local at = (mem.epoch_at or 0)
            saving = view.time >= at - S.reserve_window_min * 60000
        end
    end
    for _, k in ipairs(V.resources) do
        r[k] = saving and (view.next_epoch_cost[k] + (both and (view.epoch2_cost or {})[k] or 0)) or 0
    end
    -- copper_reserve_pct: the early epochs' reserve scaled to what upstream's costs were (850 food for the Stone Age
    -- against our 1,122; 750 / 400 / 400 for the Copper Age against 1,584 / 844 / 844): the reserve the workers
    -- gather for and the army leaves alone is upstream's, the advance still waits for the whole cost (0: off)
    if view.epoch < FARM_EPOCH and S.copper_reserve_pct ~= nil and S.copper_reserve_pct[view.epoch + 1] ~= nil then
        local pct = S.copper_reserve_pct[view.epoch + 1]
        local pct2 = S.copper_reserve_pct[view.epoch + 2] or pct
        for _, k in ipairs(V.resources) do
            r[k] = saving and (view.next_epoch_cost[k] * pct // 100 + (both and (view.epoch2_cost or {})[k] * pct2 // 100 or 0)) or 0
        end
    end
    -- copper_plan=next_epoch (econ.c hook_add_resource): from min_workforce on, epoch_share of the income goes into
    -- the epoch account until it holds the next epoch's cost; the citizens spend the rest and never wait for the
    -- epoch (the account is out of their reach: the civil budget and the engine's reserve below); the army sees
    -- the whole cost held as before
    if S.copper_plan == "next_epoch" and view.epoch < FARM_EPOCH then
        if mem.acc_epoch ~= view.epoch then
            mem.acc_epoch = view.epoch
            mem.epoch_acc = {}
        end
        mem.epoch_acc = mem.epoch_acc or {}
        local dt = math.max(view.time - (mem.acc_t or view.time), 0)
        mem.acc_t = view.time
        for _, k in ipairs(V.resources) do
            local acc = mem.epoch_acc[k] or 0
            if saving and dt > 0 and dt <= 5000 then
                -- the fraction carried (a step's share of a minute's income is under 1: it truncated to 0 before)
                mem.acc_frac = mem.acc_frac or {}
                local f = (mem.acc_frac[k] or 0) + view.income[k] * dt * S.epoch_share
                acc = acc + f // (60000 * 100)
                mem.acc_frac[k] = f % (60000 * 100)
            end
            mem.epoch_acc[k] = math.min(acc, view.next_epoch_cost[k])
        end
    else
        mem.epoch_acc = nil
    end
    if saving and view.epoch >= FARM_EPOCH then
        -- reserve_plan (econ.c spend_reserve): food is renewable from farms, so each resource keeps only what its
        -- income cannot bring back over the horizon: the slowest shortfall's time, 10 minutes at most
        local h = 0
        for _, k in ipairs(V.resources) do
            local miss = r[k] - view.stock[k]
            if miss > 0 then
                local rate = view.income[k]
                local t = rate > 1 and (miss * 10 + rate - 1) // rate or 100   -- tenths of a minute
                h = math.max(h, math.min(t, 100))
            end
        end
        for _, k in ipairs(V.resources) do
            local more = view.income[k] * 8 * h // 100   -- 80% of the income over h tenths of a minute
            r[k] = math.max(r[k] - more, 0)
        end
    end
    -- [priority.c] the epoch row of the list; the towns' reserve (towns.c, expand.c) and the navy's account (navy.c
    -- navy_hold) are rows of their own there (priority.lua), each held from the spenders upstream names
    mem.epoch_reserve = r
    return r, saving
end

-- the military account: a share of the start stock, then of every income; what it may spend now per resource
-- (within the stock above what the list holds from a military unit), the hoard above reserve + cap free for the
-- army too; civil: what a civilian unit (a citizen) sees. [priority.c] reserve: the list's hold from a military
-- unit (S_MIL), reserve_civ: from a civilian one (0)
function M.accounts(view, mem, reserve, reserve_civ)
    mem.early_epoch = view.epoch
    mem.mil = mem.mil or {}
    local mil = mem.mil
    if not mem.mil_init then
        mem.mil_init = true
        for _, r in ipairs(V.resources) do
            mil[r] = view.stock[r] * S.military_start_share // 100
        end
    end
    local share = view.epoch >= FARM_EPOCH and S.military_income_share_farms or S.military_income_share
    -- early_army (econ.c hook_add_resource): no army in the first epoch (share 0 unless early_soldiers), then
    -- food-free ranged soldiers: the army's food share small (early_army_food_share) and at most 300 held before
    -- farms, so the citizens keep their food
    local food_share = share
    if S.early_army and view.epoch < FARM_EPOCH then
        if view.epoch == 0 and (S.early_soldiers or 0) == 0 then
            share, food_share = 0, 0
        else
            food_share = (mem.mil or {}).food ~= nil and mem.mil.food >= 300 and 0 or S.early_army_food_share
        end
    end
    -- the income over the last 60 s, one second's worth a step
    local dt = math.max(view.time - (mem.mil_t or view.time), 0)
    mem.mil_t = view.time
    if dt > 0 and dt <= 5000 then
        for _, r in ipairs(V.resources) do
            -- the fraction carried (a step's share of a minute's income is under 1: it truncated to 0 before, so the
            -- account never grew past its start share)
            mem.mil_frac = mem.mil_frac or {}
            local f = (mem.mil_frac[r] or 0) + view.income[r] * dt * (r == "food" and food_share or share)
            mil[r] = (mil[r] or 0) + f // (60000 * 100)
            mem.mil_frac[r] = f % (60000 * 100)
        end
    end
    local avail, civil = {}, {}
    for _, r in ipairs(V.resources) do
        local above = math.max(view.stock[r] - reserve[r], 0)
        local cap = (r == "food" or r == "wood") and S.hoard_cap or S.hoard_cap_mineral
        mil[r] = math.min(mil[r] or 0, above + cap)   -- the account never outgrows what could be spent
        local free = math.max(above - cap, 0)
        avail[r] = math.min(above, (mil[r] or 0) + free)
        if S.early_army and view.epoch < FARM_EPOCH then
            -- early_army (docs/economy.md): before farms the military account belongs to the army - it pays out
            -- whatever the epoch reserve is (the citizens never spend its food, above); f10's Stone Age army stayed
            -- at 0 with the account under the Copper reserve
            avail[r] = math.min(view.stock[r], mil[r] or 0)
        end
        civil[r] = math.max(view.stock[r] - reserve_civ[r], 0)
        if S.early_army and r == "food" and (mem.early_epoch or 0) < FARM_EPOCH then
            civil[r] = math.max(civil[r] - (mil[r] or 0), 0)   -- early_army: the citizens never spend the army's food
        end
    end
    return avail, civil
end

-- the army pays: the account loses the cost (the stock pays through the verb)
function M.pay_military(mem, cost, n)
    for _, r in ipairs(V.resources) do
        mem.mil[r] = math.max((mem.mil[r] or 0) - cost[r] * n, 0)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The worker plan (econ_targets, food_plan_needs, citizens.c cz_targets)

-- the measured income per gatherer and minute (at least 1)
local function per_worker(view, r)
    local g = view.gatherers[r] + (r == "food" and view.boats or 0)
    if g <= 0 then
        return r == "food" and 6 or r == "wood" and 5 or 4
    end
    return math.max(view.income[r] // g, 1)
end

-- the targets: land gatherers wanted per resource
function M.workers(view, mem, reserve, wants)
    local total = view.citizens
    if total < 6 then
        return nil
    end
    local late = view.epoch >= FARM_EPOCH
    local need = {}
    local citizens_more = math.min(math.max((wants.citizens or 0) - view.citizens, 0), 10)
    -- food: the reserve plus 300 over the stock, and the citizens' own (50 a citizen for the next 10 while short)
    need.food = reserve.food + 300 - view.stock.food + 50 * citizens_more + (wants.food or 0)
    -- wood: the buildings queued and wanted (a hundred over), the fields' wood
    local w = math.max(reserve.wood, (wants.wood or 0) + 300) + 100
    need.wood = w - view.stock.wood
    -- stone, gold, iron: the demand (the army's and the buildings') beyond the reserve up to the margin, the
    -- reserve's shortfall at least
    for _, r in ipairs(MINERALS) do
        local d = (wants[r] or 0) + reserve[r] + S.mineral_worker_margin // 2
        if d > reserve[r] + S.mineral_worker_margin then
            d = reserve[r] + S.mineral_worker_margin
        end
        need[r] = d - view.stock[r]
        if reserve[r] - view.stock[r] > need[r] then
            need[r] = reserve[r] - view.stock[r]
        end
    end
    need.stone = need.stone + (wants.tower_stone or 0)
    -- by worker-minutes: the shortfall over the rate per worker, so all arrive at about the same time
    local weight, wsum, sum = {}, 0, 0
    for _, r in ipairs(V.resources) do
        need[r] = math.max(need[r], 0)
        sum = sum + need[r]
        weight[r] = need[r] * 100 // per_worker(view, r)
        wsum = wsum + weight[r]
    end
    local target = {}
    if sum <= 0 or wsum <= 0 then
        -- nothing short: food and wood by the plan's floors, the rest on the metals the army lacks
        target = {food = total * 50 // 100, wood = total * 20 // 100, stone = total * 6 // 100}
        target.gold = (total - target.food - target.wood - target.stone) // 2
        target.iron = total - target.food - target.wood - target.stone - target.gold
    else
        local floor_ = {food = 2, wood = 2, stone = 0, gold = 0, iron = 0}
        local rest, given = total - 4, 0
        for _, r in ipairs(V.resources) do
            target[r] = floor_[r] + rest * weight[r] // wsum
            given = given + target[r]
        end
        local k = 1
        while given < total do   -- the rounding leftovers to the needs, round robin
            local r = V.resources[k]
            if need[r] > 0 then
                target[r] = target[r] + 1
                given = given + 1
            end
            k = k % 5 + 1
            if k == 1 and sum <= 0 then
                break
            end
        end
    end
    -- the food floor: half the workers on food, up to food_floor, before farms (the citizens' own food); from farms
    -- the fields: at least their farmers (fields + 2) when the plan has them
    local floor_food = late and math.min(total // 2, math.max((wants.fields or 0) + 2, 6))
        or math.min(total * S.food_share // 100, S.food_floor)
    if late and mem.recovering then
        floor_food = total * 2 // 3   -- RECOVERY: two thirds on food, the fields filled
    end
    mem.food_floor = floor_food   -- [metal.c] MIX keeps it (citizen_plan's FEED)
    while target.food < floor_food do
        local from = nil
        for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
            if target[r] > 2 and (from == nil or target[r] > target[from]) then
                from = r
            end
        end
        if from == nil then
            break
        end
        target[from] = target[from] - 1
        target.food = target.food + 1
    end
    -- before farms the food gatherers are capped by the places the gather verb can fill (the base game's 6 to a
    -- bush, the same cap upstream's food_bush honours; a gatherer sent beyond the room queues and is wasted - the
    -- wood piled up while the food income stood at 0): the claimed food sites' places plus the game near home. A
    -- port rule, not upstream's: upstream's game sent the rest to bushes farther out and to the hunt (food_plan
    -- NEAR, HUNT), which the claims stand in for (more patches and a wider claim while the places are short).
    -- The excess goes to wood, stone, gold and iron by the plan's floors
    if not late then
        local places = 0
        for _, st in ipairs(view.sites) do
            if st.claimed and st.resource == "food" and st.units > 0 then
                places = places + st.capacity
            end
        end
        places = places + math.min(view.hunt or 0, 6)
        mem.food_places = places
        mem.food_want_raw = target.food   -- the claims grow on the uncapped want (more patches, a wider claim)
        local excess = target.food - places
        if excess > 0 then
            target.food = places
            -- the excess by the plan's floors: wood and the towers' stone first; gold and iron only a token before
            -- the Stone Age (docs/STRATEGY_RESEARCH.md 4.2: the Copper Age's metals are not gathered before the
            -- earlier goal - a DEVIATION from the port's first split of 37 / 37, not an upstream rule either way)
            local shares = {wood = 20, stone = 6, gold = 37, iron = 37}   -- (55 / 25 / 10 / 10 in 8.38 f11: no gain)
            local given = 0
            for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
                local n = excess * shares[r] // 100
                target[r] = target[r] + n
                given = given + n
            end
            target.gold = target.gold + (excess - given)
        end
    end
    -- from farms: the mines' places near the drop sites get their workers from food beyond its floor and wood beyond
    -- its need (citizen_plan GOAL)
    if late then
        for _, r in ipairs(MINERALS) do
            local places = wants.places and wants.places[r] or 0
            while target[r] < places do
                local from = nil
                if target.food > floor_food + 2 then
                    from = "food"
                elseif target.wood > math.max(need.wood // (per_worker(view, "wood") * 2), 4) then
                    from = "wood"
                end
                if from == nil then
                    break
                end
                target[from] = target[from] - 1
                target[r] = target[r] + 1
            end
        end
    end
    return target
end

-- ---------------------------------------------------------------------------------------------------------------
-- Sites: the claims (what the finders may send gatherers to), the drop sites

local SF = require("ageofai.safety")   -- [attack.c citizens_safety] the sites in danger by the hurt spots

local function nearest_free(view, resource, patch, range, mem)
    local best = nil
    for _, s in ipairs(view.sites) do
        if not (s.claimed or s.resource ~= resource or s.units == 0 or s.danger > 0 or (patch and not s.patch)
                or s.distance > range or s.blocked == "tower" or s.blocked == "enemy_near"
                or (mem ~= nil and SF.site_dangerous(mem, s.x, s.y, view.time))) then
            -- the start resources by the town first (home), then the nearest
            if best == nil or (s.home and not best.home) or (s.home == best.home and s.distance < best.distance) then
                best = s
            end
        end
    end
    return best
end

local function claimed(view, resource, patch)
    local n, room, places = 0, 0, 0
    for _, s in ipairs(view.sites) do
        if s.claimed and s.resource == resource and s.units > 0 and (not patch or s.patch) then
            n = n + 1
            room = room + s.room
            places = places + s.capacity
        end
    end
    return n, room, places
end

local function claims(view, mem, target, order)
    local range = view.claim_range
    local patches, patch_room = claimed(view, "food", true)
    local free_patch = nearest_free(view, "food", true, range, mem)
    -- forage patches with room for the food gatherers (two at least, four at most); the claim grows when none is
    -- left in it; the food gatherers hunt when no patch is left at all (the game near home first: food_plan HUNT)
    local most = view.epoch == 0 and 3 or 5
    -- a deviation (food.c has no patch cap: upstream's citizens take the engine's nearest food): the Prehistoric
    -- workforce of min_workforce outgrows three patches' places (seed 2 against classic: 18 places for 32 food
    -- gatherers wanted, the claim never grew in 50 minutes), so the cap lifts while the patches' room is short
    local food_want = math.max(mem.food_want_raw or 0, target.food)
    if view.epoch < 2 and patch_room < food_want - view.gatherers.food then
        most = 8   -- the places short of the food gatherers wanted: more patches (upstream's "forage farther out")
    end
    if patches < 2 or (patches < most and patch_room < food_want - view.gatherers.food + 2) then
        if free_patch then
            order({verb = "claim", site = free_patch.index})
        elseif range < view.claim_range_max then
            order({verb = "claim_range", range = range + view.claim_range_step})
        end
    end
    -- a site of each mineral the gatherers want: the nearest free, more while the claimed are full (mine_choice:
    -- the free mine nearest a drop site; the engine's finders take the one nearest a drop site among the claimed).
    -- From the Copper Age the claim grows to its most while the mines are full (towns.c RESOURCE TOWNS: the home
    -- continent's mines worked out, a town further off)
    local late = view.epoch >= FARM_EPOCH
    for _, r in ipairs(MINERALS) do
        if target[r] > 0 then
            local n, room = claimed(view, r, false)
            if late and room <= 0 and range < view.claim_range_max and view.time - (mem.range_t or 0) >= 60000 then
                order({verb = "claim_range", range = range + view.claim_range_step})
                mem.range_t = view.time
            end
            if n == 0 or (room <= 0 and n < (late and 5 or 3)) then
                local site = nearest_free(view, r, false, range, mem)
                if site then
                    order({verb = "claim", site = site.index})
                elseif range < view.claim_range_max and n == 0 then
                    order({verb = "claim_range", range = range + view.claim_range_step})
                end
            end
        end
    end
    -- a claimed site with nothing left, or blocked for good, or in danger by the hurt spots (citizens_safety: the
    -- searches refuse it; claimed again once the spots are gone), is released
    for _, s in ipairs(view.sites) do
        if s.claimed and (s.units == 0 or s.blocked == "tower" or SF.site_dangerous(mem, s.x, s.y, view.time)) then
            order({verb = "release", site = s.index})
        end
    end
    return patches == 0 and free_patch == nil
end

-- drop sites (mines.c mine_drops, wood_drops, food.c CAMP; the engine's finders send gatherers only where a finished
-- drop site stands, AI_RESEARCH §8.36): a settlement at every claimed site the gatherers want whose resource has no
-- drop, food first, then wood, iron, gold, stone, nearest first; two going up at once; the forest edge too
local function reserve_wood(view)
    return view.reserve.wood or 0
end

-- a building at a spot of its own through the per-unit interface when the town's rings are full (an island town):
-- ai.find_site's tile, the three citizens nearest it build it (idle first) and stay the script's until it stands
-- (released at once, the runners took them back and the foundation stood empty: 17 granary foundations in one
-- game); one such building at a time, three minutes at most. Returns true when ordered. [packing.c] sx, sy: a
-- spot chosen already (a packed production building), no search
local function build_direct(view, mem, type, sx, sy)
    mem.direct = mem.direct or {}
    local d = mem.direct
    if d.id ~= nil then
        local u = ai.unit(d.id)
        local done = u == nil or u.built or view.time - d.t > 180000
        if u == nil and view.time - d.t < 60000 and d.x ~= nil then
            mem.direct_block = mem.direct_block or {}
            mem.direct_block[d.x * 10000 + d.y] = view.time + 600000   -- the foundation vanished at once: not there again
        end
        if not done then
            -- a builder gone idle (the engine dropped the goal): on it again
            local again = {}
            for _, b in ipairs(d.builders) do
                local bu = ai.unit(b)
                if bu ~= nil and bu.action == "idle" then
                    again[#again + 1] = b
                end
            end
            if #again > 0 then
                ai.repair(again, d.id)
            end
            return false
        end
        ai.release(d.builders)
        mem.direct = {}
        d = mem.direct
    end
    -- a tile on the town's own land (an island's rings reach across the water: a foundation its builders cannot
    -- reach is cancelled by the engine, and the order came again every minute)
    local gx, gy = sx, sy
    for _, opt in ipairs(gx ~= nil and {} or {{min = 4, max = 20, safe = true}, {min = 4, max = 20}, {min = 2, max = 30}}) do
        for r = opt.min, opt.max, 3 do
            local x, y = ai.find_site(type, view.town_x, view.town_y, {min = r, max = math.min(r + 2, opt.max), safe = opt.safe})
            if x ~= nil and ai.reach(view.town_x, view.town_y, x, y)
                    and ((mem.direct_block or {})[x * 10000 + y] or 0) <= view.time
                    and (type ~= "b_granary" or #ai.units{type = "b_granary", near = {x, y, 8}} == 0) then
                gx, gy = x, y
                break
            end
        end
        if gx ~= nil then
            break
        end
    end
    if gx == nil then
        return false
    end
    local cits = ai.units{category = "citizen", idle = true, near = {gx, gy, 60}, limit = 3}
    if #cits < 3 then
        for _, id in ipairs(ai.units{category = "citizen", near = {gx, gy, 40}, limit = 8}) do
            local have = false
            for _, c in ipairs(cits) do
                if c == id then
                    have = true
                end
            end
            local u = ai.unit(id)
            if not have and #cits < 3 and u ~= nil and u.inside == nil and u.held == "engine" and u.action ~= "build" then
                cits[#cits + 1] = id
            end
        end
    end
    if #cits == 0 then
        return false
    end
    local code, reason, n, id = ai.build(cits, type, gx, gy)
    if code == "ok" and id ~= nil then
        d.id, d.t, d.builders, d.type, d.x, d.y = id, view.time, cits, type, gx, gy
        print(string.format("build_direct type=%s at=%d,%d builders=%d id=%d", type, gx, gy, #cits, id))
        return true
    end
    ai.release(cits)
    return false
end

local function settlements(view, mem, target, order)
    if V.cost(view, "b_settlement") == nil then
        return
    end
    local entry = V.buildings(view, "b_settlement")
    local going = entry ~= nil and entry.unfinished + entry.queued or 0
    local tcs = V.buildings(view, "b_town_center")
    going = going + ((tcs ~= nil and tcs.unfinished + tcs.queued) or 0)
    local gran = V.buildings(view, "b_granary")
    going = going + ((gran ~= nil and gran.unfinished + gran.queued) or 0)
    if going >= SETTLE_AT_ONCE then
        return
    end
    mem.settle = mem.settle or {}
    mem.settle_block = mem.settle_block or {}
    mem.settle_pending = mem.settle_pending or {}
    local cands = {}
    -- [towns.c, expand.c] a site by a town or an outpost of ageofai.towns (going up or standing) is that town's: no
    -- settlement of this pass there (one or the other issues the drop site's order); nor at a site across the water
    -- (the claim range reaches over it; the coarse build sent a walker that never arrived)
    local TW = require("ageofai.towns")
    for _, s in ipairs(view.sites) do
        if s.claimed and s.units > 0 and s.danger == 0 and s.blocked ~= "tower" and s.blocked ~= "enemy_near"
                and not TW.near_town(mem, s.x, s.y, 18) and ai.reach(view.town_x, view.town_y, s.x, s.y) then
            for _, r in ipairs(V.resources) do
                if r ~= "wood" and (target[r] or 0) > 0 and (s.holds[r] or 0) > 0 and (s.drop[r] or 0) == 0 then
                    cands[#cands + 1] = {rank = SETTLE_RANK[r], dist = s.distance, site = s, key = s.x * 10000 + s.y}
                end
            end
        end
    end
    local wood = view.wood
    if wood ~= nil and (target.wood or 0) > 0 and not wood.ready and not wood.has_drop then
        cands[#cands + 1] = {rank = SETTLE_RANK.wood, dist = wood.distance, tile = wood, key = wood.x * 10000 + wood.y}
        mem.wd_want_t = view.time   -- [priority.c] the forest row (mines.c wood_reserve: a Settlement wanted at the edge)
    end
    table.sort(cands, function(a, b)
        if a.rank ~= b.rank then
            return a.rank < b.rank
        end
        if a.dist ~= b.dist then
            return a.dist < b.dist
        end
        return a.key < b.key
    end)
    for _, c in ipairs(cands) do
        if going >= SETTLE_AT_ONCE then
            break
        end
        local last = mem.settle[c.key] or 0
        if (view.time - last >= SETTLE_WAIT or last == 0) and (mem.settle_block[c.key] or 0) <= view.time then
            local at
            -- a town centre instead of a settlement at a far mineral site from the Copper Age when the wood allows
            -- (towns.c: a town with production of its own where the mines are)
            local kind = "b_settlement"
            if c.site ~= nil and view.epoch >= FARM_EPOCH and c.site.distance >= 30 and c.site.resource ~= "food"
                    and V.cost(view, "b_town_center") ~= nil and view.stock.wood >= 600 + reserve_wood(view) then
                kind = "b_town_center"
            end
            if c.site ~= nil then
                at = order({verb = "build", type = kind, near = "site", site = c.site.index})
            else
                at = order({verb = "build", type = "b_settlement", near = "tile", x = c.tile.x, y = c.tile.y})
            end
            mem.settle_pending[#mem.settle_pending + 1] = {at = at, key = c.key}
            mem.settle[c.key] = view.time
            going = going + 1
        end
    end
    -- forget old marks
    for _, k in ipairs(ai.keys(mem.settle)) do
        if view.time - mem.settle[k] > 10 * 60000 then
            mem.settle[k] = nil
        end
    end
    for _, k in ipairs(ai.keys(mem.settle_block)) do
        if mem.settle_block[k] <= view.time then
            mem.settle_block[k] = nil
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The fronts and the towers (defence.c def_refresh / defence_tick, early.c early_towers)

-- the direction of the front: towards the nearest enemy building known (the memory), else the map's centre
function M.front(view, mem)
    local tx, ty = view.town_x, view.town_y
    local bld = ai.memory{buildings = true, near = {tx, ty, 200}, limit = 1}
    local fx, fy
    if #bld > 0 then
        fx, fy = bld[1].x - tx, bld[1].y - ty
        mem.front_enemy = bld[1].player
    else
        local map = ai.map()
        fx, fy = map.width // 2 - tx, map.height // 2 - ty
    end
    local len = isqrt(fx * fx + fy * fy)
    if len < 4 then
        return 1, 0, 0
    end
    return fx, fy, len
end

-- a tower spot at the front: the slots defence_distance tiles out, 3 tiles apart across the front, then one ring in
local function front_spot(view, mem, tower, fx, fy, len)
    local tx, ty = view.town_x, view.town_y
    local d = S.defence_distance
    local px, py = -fy, fx
    for _, ring in ipairs({d, d - 3}) do
        for _, k in ipairs({0, 1, -1, 2, -2}) do
            local x = tx + fx * ring // len + px * (3 * k) // len
            local y = ty + fy * ring // len + py * (3 * k) // len
            local others = ai.units{category = "building", near = {x, y, 2}}
            local taken = false
            for _, id in ipairs(others) do
                local u = ai.unit(id)
                if u ~= nil and u.armed then
                    taken = true
                end
            end
            if not taken then
                local sx, sy = ai.find_site(tower, x, y, {min = 0, max = 2, safe = true})
                if sx == nil then
                    sx, sy = ai.find_site(tower, x, y, {min = 0, max = 2})
                end
                if sx ~= nil then
                    return sx, sy
                end
            end
        end
    end
    return nil
end

-- early_towers: the two core spots by the town centre, early_dist out: towards the front, and across it on the
-- citizens' side
local function core_spot(view, mem, tower, fx, fy, len)
    local tx, ty = view.town_x, view.town_y
    local px, py = -fy, fx
    local cits = ai.units{category = "citizen", near = {tx, ty, 15}}
    local cols = ai.get(cits, {"x", "y"})
    local sx, sy = 0, 0
    for k = 1, #cits do
        if cols.x[k] ~= nil then
            sx, sy = sx + cols.x[k] - tx, sy + cols.y[k] - ty
        end
    end
    if px * sx + py * sy < 0 then
        px, py = -px, -py
    end
    local slots = {{fx, fy}, {px, py}}
    for k = 1, S.early_core do
        local dx, dy = slots[k][1], slots[k][2]
        local x, y = tx + dx * S.early_dist // len, ty + dy * S.early_dist // len
        local near = ai.units{category = "building", near = {x, y, 2}}
        local has = false
        for _, id in ipairs(near) do
            local u = ai.unit(id)
            if u ~= nil and u.armed then
                has = true
            end
        end
        if not has then
            local ox, oy = ai.find_site(tower, x, y, {min = 0, max = 5})
            if ox ~= nil then
                return ox, oy
            end
        end
    end
    return nil
end

-- [defence.c enclose] a free spot of the ring's tower spots: no armed building within 2, placeable within 2
local function ring_spot(mem, tower)
    for _, p in ipairs(mem.ring_spots) do
        local x, y = p[1], p[2]
        local others = ai.units{category = "building", near = {x, y, 2}}
        local taken = false
        for _, id in ipairs(others) do
            local u = ai.unit(id)
            if u ~= nil and u.armed then
                taken = true
            end
        end
        if not taken then
            local sx, sy = ai.find_site(tower, x, y, {min = 0, max = 2})
            if sx ~= nil then
                return sx, sy
            end
        end
    end
    return nil
end

local function towers(view, mem, order, above)
    local tower = M.tower_class(view)
    if tower == nil or view.town_x < 0 then
        return 0
    end
    local cost = V.cost(view, tower)
    local n, finished, busy = tower_counts(view)
    local stone_wanted = 0
    local fx, fy, len = M.front(view, mem)
    -- the core: in the first two epochs the first two towers stand by the town centre
    if S.early_towers and view.epoch <= 1 and n < S.early_core then
        stone_wanted = (S.early_core - n) * cost.cost.stone
        if not busy and view.time - (mem.tower_t or 0) >= 20000 and above.stone >= cost.cost.stone then
            local x, y = core_spot(view, mem, tower, fx, fy, len)
            if x ~= nil then
                order({verb = "build", type = tower, near = "tile", x = x, y = y})
                mem.tower_t = view.time
                return stone_wanted
            end
        end
    end
    -- the front and the outposts: one a minute while the stone allows (tower_stone_keep kept back), none while
    -- one is queued or going up
    local max_towers = 4 + S.towers_max_epoch * view.epoch
    if not S.defence_towers or busy or n >= max_towers or view.time - (mem.tower_t or 0) < 60000
            or view.citizens < 8 then
        return stone_wanted
    end
    -- [defence.c enclose] encl_owed: the extra towers wait for the open ring's stone (up to 400; walls.lua)
    if above.stone < cost.cost.stone + S.tower_stone_keep + math.min(mem.wall_owed or 0, 400) then
        return stone_wanted + cost.cost.stone
    end
    if P.refuses(view, cur_memory, tower) ~= nil then   -- [priority.c] a row above holds its price
        return stone_wanted
    end
    -- an outpost: a worked mineral site 12+ tiles from the town with no tower (3 tiles towards the front)
    for _, s in ipairs(view.sites) do
        if s.claimed and s.workers > 0 and not s.has_tower and s.distance >= 12 and s.resource ~= "food"
                and s.danger == 0 and s.blocked ~= "tower" and s.blocked ~= "enemy_near" then
            order({verb = "build", type = tower, near = "site", site = s.index})
            mem.tower_t = view.time
            return stone_wanted
        end
    end
    -- [defence.c enclose] def_slot with the ring on: the slots are the spots along the ring (walls.lua writes
    -- mem.ring_spots: on the coast 1 tile inland and 5 apart, in the octagon 2 inside and 6 apart), the outposts
    -- above after them, no front slots
    local x, y
    if mem.ring_spots ~= nil then
        x, y = ring_spot(mem, tower)
    else
        x, y = front_spot(view, mem, tower, fx, fy, len)
    end
    if x ~= nil then
        order({verb = "build", type = tower, near = "tile", x = x, y = y})
        mem.tower_t = view.time
    end
    return stone_wanted
end

-- ---------------------------------------------------------------------------------------------------------------
-- Buildings: production, granaries and farms, docks, the SOL

-- the production buildings: a barracks early (the home guard), an archery range in epochs 1-3 (early_requirements),
-- a stable from the Copper Age; army_plan BUILD: one more of the kind that trains the soldier picked when its
-- producers are busy and the account holds 600, up to 2 + workers / 15
-- [econ.c] IDLE CITIZENS HOME (citizens_home): every 3 s, a citizen the engine holds with no goal more than 15 tiles
-- from its nearest finished town (Town Center, Capitol, Settlement), idle at the last look too (between two jobs is
-- not), walks to the nearest finished town on its land with no enemy soldiers seen within 12 tiles (none safe: it
-- stays), to the spot 4 tiles out on the citizen's side (the building's own tile is taken); then it is the
-- engine's again (the assignment puts it to work). ENGINE: upstream's parts (1) and (3) - the base point moved to
-- the biggest town (the game set it from the first unit it found) and the stacked foundations removed - are the
-- original game's bugs, not this engine's (the town centre is view.town_x/y, the build runner lays one foundation);
-- its (2) "walking to within 3 tiles of the base" is that base point's too. Citizens the script holds (a landing,
-- an outpost's ferry, a new town) are not the engine's and are left alone.
local TOWN_TYPES = {"b_settlement", "b_town_center", "b_capitol"}
-- (the squared distance of two tiles; this function never ran before ai.units took a list of types, and the global
-- it called did not exist)
local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end
local function citizens_home(view, mem)
    if not S.citizens_home or view.town_x < 0 or view.time - (mem.home_t or 0) < 3000 then
        return
    end
    mem.home_t = view.time
    local towns = {}
    for _, id in ipairs(ai.units{type = TOWN_TYPES, built = true}) do
        local u = ai.unit(id)
        if u ~= nil then
            towns[#towns + 1] = u
        end
    end
    if #towns == 0 then
        return   -- no town left
    end
    local idle = ai.units{category = "citizen", idle = true, held = "engine"}
    local was = mem.home_was or {}
    local now_idle, safe, sent, stay = {}, {}, 0, 0
    local cols = ai.get(idle, {"x", "y", "inside"})
    for k, id in ipairs(idle) do
        local x, y = cols.x[k], cols.y[k]
        if x ~= nil and cols.inside[k] == nil then
            local nd = nil
            for _, t in ipairs(towns) do
                local d = d2(t.x, t.y, x, y)
                if nd == nil or d < nd then
                    nd = d
                end
            end
            if nd > 15 * 15 then
                now_idle[#now_idle + 1] = id
                if was[id] then   -- idle away twice in a row
                    local best, bd = nil, nil
                    for i, t in ipairs(towns) do
                        if ai.reach(t.x, t.y, x, y) then
                            if safe[i] == nil then
                                safe[i] = #ai.seen{enemy = true, armed = true, category = "soldier", near = {t.x, t.y, 12}, limit = 1} == 0
                            end
                            local d = d2(t.x, t.y, x, y)
                            if safe[i] and (bd == nil or d < bd) then
                                best, bd = t, d
                            end
                        end
                    end
                    if best == nil then
                        stay = stay + 1
                    else
                        -- beside the town on the citizen's side: the building's own tile is taken
                        local dx, dy = x - best.x, y - best.y
                        local l = isqrt(dx * dx + dy * dy)
                        local gx = best.x + (l > 0 and dx * 4 // l or 4)
                        local gy = best.y + (l > 0 and dy * 4 // l or 0)
                        local code = ai.move({id}, gx, gy)
                        ai.release({id})
                        if code == "ok" then
                            sent = sent + 1
                        end
                    end
                end
            end
        end
    end
    local set = {}
    for _, id in ipairs(now_idle) do
        set[id] = true
    end
    mem.home_was = set
    if sent > 0 or stay > 0 then
        mem.home_sent = (mem.home_sent or 0) + sent
        if view.time - (mem.home_log_t or -100000) >= 60000 then
            mem.home_log_t = view.time
            print(string.format("citizens_home sent=%d stay=%d idle_away=%d towns=%d total=%d", sent, stay, #now_idle, #towns, mem.home_sent))
        end
    end
end

local function production(view, mem, order, army, above)
    local want = {}
    want.b_barracks = view.citizens >= 8 and 1 or 0
    if view.epoch >= 1 and view.epoch <= 3 then
        want.b_archery_range = 1
    end
    if view.epoch >= FARM_EPOCH and view.citizens >= 30 then
        want.b_stable = 1
    end
    if army ~= nil and army.producer ~= nil and army.busy then
        local have = V.buildings(view, army.producer)
        local n = V.total(have)
        local most = 2 + view.citizens // 15
        local account = 0
        for _, r in ipairs(V.resources) do
            account = account + (mem.mil[r] or 0)
        end
        if n < most and account >= 600 then
            want[army.producer] = math.max(want[army.producer] or 0, n + 1)
        end
    end
    for _, t in ipairs(ai.keys(want)) do
        local c = V.cost(view, t)
        if c ~= nil then
            local have = V.buildings(view, t)
            local going = have ~= nil and have.unfinished + have.queued > 0
            local ok = true
            for _, r in ipairs(V.resources) do
                ok = ok and above[r] >= c.cost[r]
            end
            if not going and V.total(have) < want[t] and ok and P.refuses(view, cur_memory, t) == nil then   -- [priority.c]
                -- [packing.c] PACKED BUILDINGS (tight_build): a production building goes to the spot touching the
                -- most of its kind nearest the base, through ai.build (the verb's placement keeps a tile free round
                -- every building); no spot, or the direct build busy: the verb's search as before
                local packed = false
                if S.tight_build and PK.is_type(t) and view.time - ((mem.pack_t or {})[t] or -100000) >= 10000 then
                    mem.pack_t = mem.pack_t or {}
                    mem.pack_t[t] = view.time
                    local px, py = PK.spot(view, mem, t)
                    if px ~= nil then
                        packed = build_direct(view, mem, t, px, py)
                    end
                end
                if packed then
                    mem.packed_built = (mem.packed_built or 0) + 1
                elseif (mem.nosite or {})[t] ~= nil and view.time - mem.nosite[t] < 600000 then
                    if view.time - (mem.direct_t or 0) >= 30000 then
                        mem.direct_t = view.time
                        build_direct(view, mem, t)
                    end
                else
                    mem.build_pending = mem.build_pending or {}
                    mem.build_pending[#mem.build_pending + 1] = {at = order({verb = "build", type = t, near = "town"}), type = t}
                end
            end
        end
    end
end

-- granaries and fields from the Copper Age (build_fields, more_granaries): a granary while the food workers wanted
-- exceed the fields (8 a granary, fewer than 1 + target / 8, at most 8); the fields as the wood pays (50 kept each)
-- [priority.c] THE SILOS' ROW (econ.c farm_plan, farm_first; the player 2026-11-09: "the second copper age starts
-- researching the silo with its 525 wood should be put in the priority list and good is", "a second should follow
-- soon after"). 525 wood: a Granary's 125 and 8 fields of 50. Due from the moment the Copper Age is saved for, one
-- silo at a time, the second when the first's fields are built, whatever the food target says. What is still due
-- stays held (mem.farm_hold; mem.fields_open: the fields going up, for field_wood). No progress for 6 minutes (no
-- room for a silo, no builders): nothing held until there is some. Both silos' fields done: over for the game.
local FARM_HOLD_MS = 360000
local function farm_plan(view, mem, saving)
    local gran, farm = V.buildings(view, "b_granary"), V.buildings(view, "b_farm")
    local silos = V.total(gran)
    local done, open = farm ~= nil and farm.finished or 0, farm ~= nil and farm.unfinished or 0
    mem.fields_open = open
    local due = S.farm_first and not mem.farm_over and (view.epoch >= FARM_EPOCH or (view.epoch == FARM_EPOCH - 1 and saving))
    local hold = 0
    local cg, cf = V.cost(view, "b_granary"), V.cost(view, "b_farm")
    if due and cg ~= nil and cf ~= nil then
        local gw, fw = cg.cost.wood, cf.cost.wood
        local n = math.min(silos, 2)
        local unlaid = math.max(8 * n - done - open, 0)   -- a silo's fields come as foundations when it stands
        hold = fw * (unlaid + open)
        if silos < 2 and open == 0 and unlaid == 0 then
            hold = hold + gw + 8 * fw
        end
        if hold > gw + 8 * fw then
            hold = gw + 8 * fw
        end
        if silos >= 2 and done >= 16 then
            mem.farm_over = true
            hold = 0
        end
    end
    local key = silos * 256 + done   -- progress: a silo or a finished field
    if not due or key ~= (mem.farm_key or -1) or (mem.farm_key_t or 0) == 0 then
        mem.farm_key_t = view.time > 0 and view.time or 1
    end
    if hold > 0 and view.time - mem.farm_key_t > FARM_HOLD_MS then
        hold = 0
    end
    if key ~= (mem.farm_key or -1) and silos <= 3 and hold ~= (mem.farm_hold or 0) then
        print(string.format("priority farms silos=%d done=%d open=%d wood=%d held=%d", silos, done, open, view.stock.wood, hold))
    end
    mem.farm_key = key
    mem.farm_hold = hold
end

local function farms(view, mem, order, target, above)
    if view.epoch < FARM_EPOCH then
        return 0
    end
    local gran = V.buildings(view, "b_granary")
    local farm = V.buildings(view, "b_farm")
    local fields = V.total(farm)
    local granaries = V.total(gran)
    local want_gran = math.min(1 + math.max(target.food or 0, 8) // 8, 8)
    if V.cost(view, "b_granary") ~= nil and granaries < want_gran and (gran == nil or gran.unfinished + gran.queued == 0)
            and (view.gatherers.food + 6 <= (target.food or 0) or granaries == 0) and above.wood >= 100 then
        -- the town's rings full (an island town: the verb found no site near the town nor round a tile): the
        -- building placed through the per-unit interface at a spot of its own
        if gran ~= nil and gran.queued > 0 and gran.unfinished == 0 then
            mem.gran_queued_t = mem.gran_queued_t or view.time
        else
            mem.gran_queued_t = nil
        end
        local stalled = mem.gran_queued_t ~= nil and view.time - mem.gran_queued_t > 180000
        if ((mem.nosite or {}).b_granary ~= nil and view.time - mem.nosite.b_granary < 600000) or stalled then
            -- (S.direct_granary; the engine's footprint code faulted on a site at the map's edge, fixed)
            if S.direct_granary and view.time - (mem.direct_t or 0) >= 120000 then
                mem.direct_t = view.time
                build_direct(view, mem, "b_granary")
            end
        else
            mem.build_pending = mem.build_pending or {}
            mem.build_pending[#mem.build_pending + 1] = {at = order({verb = "build", type = "b_granary", near = "town"}), type = "b_granary"}
        end
    end
    local fc = V.cost(view, "b_farm")
    local finished_gran = gran ~= nil and gran.finished or 0
    -- a granary placed through ai.build comes without its field foundations: the replant command lays them (once
    -- every two minutes per granary while it has fewer than 8 fields within 5 tiles)
    mem.replant = mem.replant or {}
    if S.replant_fields and finished_gran > 0 and view.time - (mem.replant_t or 0) >= 20000 then
        mem.replant_t = view.time
        for _, id in ipairs(ai.units{type = "b_granary", built = true}) do
            local g = ai.unit(id)
            if g ~= nil and #ai.units{type = "b_farm", near = {g.x + 1, g.y + 1, 5}} < 8
                    and view.time - (mem.replant[id] or -1000000) >= 120000 then
                ai.replant(id)
                mem.replant[id] = view.time
                mem.replants = (mem.replants or 0) + 1
                break
            end
        end
    end
    -- the fields' foundations count as unfinished: the order finishes them (the classic FindSite takes a foundation)
    if fc ~= nil and finished_gran > 0 and (farm == nil or farm.finished < finished_gran * 8) and (farm == nil or farm.queued < 2)
            and above.wood >= fc.cost.wood + 50 then
        -- a granary lays its eight fields as foundations: `near = town` finishes the next one (the classic FindSite
        -- finds the foundations; rings round the granary are refused)
        order({verb = "build", type = "b_farm", near = "town"})
    end
    return fields
end

-- the next epoch's SOL: one building at a time that counts toward it and is not built yet, the cheapest first
local function sol(view, mem, order)
    if view.sol_missing <= 0 then
        return
    end
    local best, best_cost, building = nil, 0, false
    for _, c in ipairs(view.costs) do
        if c.building and c.counts_toward_epoch and not NOT_FOR_SOL[c.type] then
            local have = V.buildings(view, c.type)
            if have ~= nil and have.unfinished + have.queued > 0 then
                building = true
            end
            if V.total(have) == 0 then
                local s = V.sum(c.cost)
                if best == nil or s < best_cost then
                    best, best_cost = c, s
                end
            end
        end
    end
    if best ~= nil and not building and P.refuses(view, cur_memory, best.type) == nil then   -- [priority.c]
        order({verb = "build", type = best.type, near = "town"})
    end
end

-- the gathering upgrades, one every 15 s (gather_research)
local function research(view, mem, order)
    mem.research_done = mem.research_done or {}
    mem.research_wait = mem.research_wait or {}
    local r = mem.research_at ~= nil and view.results[mem.research_at] or nil
    if r ~= nil and r.verb == "research" and mem.research_tech ~= nil then
        if r.code == "done" or r.code == "queued" then
            mem.research_done[mem.research_tech] = true
        elseif r.reason == "unknown_class" or r.reason == "bad_argument" then
            mem.research_done[mem.research_tech] = true   -- not in this data
        else
            mem.research_wait[mem.research_tech] = view.time + (r.reason == "not_in_epoch" and 300000 or 60000)
        end
    end
    mem.research_at = nil
    mem.research_tech = nil
    if view.time - (mem.research_t or 0) < 15000 then
        return
    end
    for _, t in ipairs(GATHER_TECHS) do
        if not mem.research_done[t] and (mem.research_wait[t] or 0) <= view.time then
            order({verb = "research", tech = t})
            mem.research_at = #mem.orders_ref
            mem.research_tech = t
            mem.research_t = view.time
            return
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step

-- the citizens wanted: the game's share of the cap before farms (min_workforce at least), from farms the
-- citizen_plan's goal: the claimed sites' places, the food and wood gatherers, the idle and 4 builders, within the
-- cap less the army's share
local function citizens_wanted(view, target)
    local cap = view.population_cap
    local game = cap * S.citizen_share // 100
    if view.epoch < FARM_EPOCH then
        -- before farms the game's own citizens wanted (citizens.c cz_targets runs from the Copper Age); min_workforce
        -- is no cap: from it on the reserve holds the epochs' food and the citizens wait on what is left
        return game
    end
    local places = 0
    for _, s in ipairs(view.sites) do
        if s.claimed and s.units > 0 and s.resource ~= "food" then
            places = places + s.capacity
        end
    end
    local goal = places + (target and (target.food + target.wood) or view.gatherers.food + view.gatherers.wood) + 4
    local lim = cap - cap * S.citizen_army_share // 100
    goal = math.min(goal, lim)
    return math.max(goal, game)
end

-- the economy's orders for this step. army: {producer=, busy=, wants={res=amount}} from the army side (the demand
-- its training puts on the workers, the building it would like more of). Returns the budget: {reserve, avail (the
-- army's), civil, saving, target}
function M.step(view, memory, orders, army)
    memory.eco = memory.eco or {}
    local mem = memory.eco
    cur_memory = memory
    mem.orders_ref = orders
    local function order(o)
        orders[#orders + 1] = o
        return #orders
    end
    local epoch_reserve, saving = M.reserve(view, mem)
    farm_plan(view, mem, saving)
    -- [priority.c] the list: what each spender may touch. reserve: a civilian unit's hold (every row; the workers
    -- gather for all of it); hold_mil: a military unit's; order: a priority order's stock (the siege, the bombers);
    -- ship: a warship's (the stock but the rows marked apart - the navy row is its own); build / build_econ: a
    -- building's (the silos' wood; an economy building passes it)
    local reserve = P.hold(view, memory, 0)
    local hold_mil, apart_mil = P.hold(view, memory, P.S_MIL)
    for _, r in ipairs(V.resources) do
        hold_mil[r] = hold_mil[r] + apart_mil[r]
    end
    -- copper_plan=next_epoch before farms: a citizen's hold is the epoch account, not the epoch's cost
    local reserve_civ = reserve
    if mem.epoch_acc ~= nil then
        reserve_civ = {}
        for _, r in ipairs(V.resources) do
            reserve_civ[r] = math.max(reserve[r] - epoch_reserve[r] + (mem.epoch_acc[r] or 0), 0)
        end
        -- unless the map's food runs out (econ.c): wild food is all there is before farms - the food sites within
        -- site_max_distance x wild_food_share short of the epoch's food less the account + 500 - then the epoch's
        -- food first, as the plan always did
        -- (econ.c wild_food: the explored forage sites' amounts within site_max_distance, the room not counted -
        -- the game's bush cap of 6 gatherers is the base game's, the same for both; where the food gatherers can
        -- stand is the claims' business)
        local wild = 0
        for _, st in ipairs(view.sites) do
            if st.resource == "food" and st.units > 0 and st.distance <= S.site_max_distance then
                wild = wild + (st.amount or 0)
            end
        end
        wild = wild * S.wild_food_share // 100
        mem.wild_food = wild
        if wild < view.next_epoch_cost.food - (mem.epoch_acc.food or 0) + 500 then
            reserve_civ.food = reserve.food
        end
    end
    local avail, civil = M.accounts(view, mem, hold_mil, reserve_civ)
    local order_vis = P.visible(view, memory, P.S_MIL | P.S_ORDER)
    local ship_vis = P.visible(view, memory, P.S_MIL | P.S_SHIP)
    local build_vis = P.visible(view, memory, P.S_BUILD)
    local build_econ_vis = P.visible(view, memory, P.S_BUILD | P.S_BUILD_ECON)
    -- the engine's reserve: the epoch's and the towns'; the military account is Lua's own (the verbs see the stock
    -- above the reserve). [navy.c navy_earmarked] a warship spends the navy account through the epoch's reserve
    -- (ai.train pays through the engine's check): the engine holds the epoch's less the account, Lua's budget the
    -- epoch's plus the account. DEVIATION (priority.lua): the engine checks every verb against this, so a building
    -- waits for the epoch and the towns too, where upstream's buildings spent them
    local engine = {}
    for _, r in ipairs(V.resources) do
        local towns = (mem.towns_reserve ~= nil and mem.towns_reserve[r] or 0) + (mem.expand_hold ~= nil and mem.expand_hold[r] or 0)
        local ep = mem.epoch_acc ~= nil and (mem.epoch_acc[r] or 0) or epoch_reserve[r]   -- next_epoch: the account
        engine[r] = math.max(ep + towns - (mem.navy_hold ~= nil and mem.navy_hold[r] or 0), 0)
    end
    local differs = false
    for _, r in ipairs(V.resources) do
        differs = differs or engine[r] ~= view.reserve[r]
    end
    if differs then
        order({verb = "reserve", food = engine.food, wood = engine.wood, stone = engine.stone, gold = engine.gold,
               iron = engine.iron})
    end
    -- the epoch: the tower upgrade, then the advance as soon as it is paid (rush; the timer with timer pacing), the
    -- requirements asked for early (the SOL buildings while saving)
    local upgrade = V.tower_upgrade(view, mem)
    if upgrade ~= nil then
        order(upgrade)
        mem.tower_up_at = #orders
    end
    if mem.epoch_seen ~= view.epoch then
        mem.epoch_seen = view.epoch
        mem.epoch_at = view.time + (S.epoch_min + ai.random(0, S.epoch_max - S.epoch_min)) * 60000
    end
    if saving and view.sol_missing == 0 and not view.advancing then
        local paid = true
        for _, r in ipairs(V.resources) do
            paid = paid and view.stock[r] >= view.next_epoch_cost[r]
        end
        local timer_ok = S.epoch_pacing == "rush" or view.epoch < FARM_EPOCH or view.time >= (mem.epoch_at or 0)
        if paid and timer_ok then
            order({verb = "advance"})
        end
    end

    -- citizens: two at a time from each town centre while the civil budget keeps a citizen's food
    local citizen = V.cost(view, "citizen")
    local want_citizens = citizens_wanted(view, mem.target)
    -- [bunker.c] shelf_wanted: while the shelf works or holds citizens, no more than there are (on the map or in it)
    if memory.shelf_hold then
        want_citizens = math.min(want_citizens, view.citizens)
    end
    local making = citizen and citizen.in_production or 0
    local waiting = citizen and citizen.requested or 0
    local planned = view.citizens + making + waiting
    local cit_food = mem.recovering and view.stock.food or civil.food   -- RECOVERY: the whole stock for citizens
    if citizen and waiting == 0 and making < 2 * citizen.producers and planned < want_citizens
            and view.population < view.population_cap and not view.advancing and cit_food >= citizen.cost.food then
        order({verb = "train", type = "citizen", n = math.min(want_citizens - planned, 2)})
    end

    local target = mem.target
    if mem.direct ~= nil and mem.direct.id ~= nil then
        local u = ai.unit(mem.direct.id)
        if u == nil or u.built or view.time - mem.direct.t > 180000 then
            ai.release(mem.direct.builders)
            if u ~= nil and u.built then
                print(string.format("build_direct done type=%s", mem.direct.type))
            end
            mem.direct = {}
        end
    end
    -- a building refused no_site near the town the step before: next time at a spot of its own (ai.find_site)
    mem.nosite = mem.nosite or {}
    for _, p in ipairs(mem.build_pending or {}) do
        local r = view.results[p.at]
        if r ~= nil and r.verb == "build" and r.reason == "no_site" then
            mem.nosite[p.type] = view.time
        end
    end
    mem.build_pending = {}
    -- a settlement refused no_site the step before: its spot is passed over for five minutes
    for _, p in ipairs(mem.settle_pending or {}) do
        local r = view.results[p.at]
        if r ~= nil and r.verb == "build" and r.reason == "no_site" then
            mem.settle_block = mem.settle_block or {}
            mem.settle_block[p.key] = view.time + 300000
        elseif r ~= nil and r.verb == "build" and r.reason == "unexplored" then
            mem.settle[p.key] = 0   -- a walker is on its way: ordered again next pass
        end
    end
    mem.settle_pending = {}
    -- the food gatherers hunt while the patches with a drop site cannot take them (the game near home first)
    for _, r in ipairs(view.results) do
        if r.verb == "gather" and r.reason == "no_dropoff" then
            mem.no_drop_t = view.time
        end
    end
    if view.time - (mem.pass_t or 0) >= PASS_MS or (mem.pass_t or 0) == 0 then
        mem.pass_t = view.time
        -- [defence.c enclose] encl_stone: the open ring's walls + 200 for repairs (walls.lua writes mem.wall_stone)
        local wants = {citizens = want_citizens, tower_stone = (mem.tower_stone or 0) + (mem.wall_stone or 0), fields = mem.fields or 0}
        if army ~= nil and army.wants ~= nil then
            for _, r in ipairs(V.resources) do
                wants[r] = (wants[r] or 0) + (army.wants[r] or 0)
            end
        end
        -- the places of the claimed mines near drop sites (citizen_plan GOAL)
        wants.places = {}
        for _, s in ipairs(view.sites) do
            if s.claimed and s.units > 0 and s.resource ~= "food" and s.has_drop then
                local r = s.resource
                wants.places[r] = (wants.places[r] or 0) + s.capacity
            end
        end
        target = M.workers(view, mem, reserve, wants)
        -- [metal.c] workers_to_metals (army.c MIX): from piled-up food to the wood, gold or iron the army lacks
        MT.workers(view, memory, target, mem.food_floor, wants.places, epoch_reserve)
        if target ~= nil and mem.target ~= nil then
            -- hysteresis: a target moves by 5 a pass at most (the stock swings with every purchase); food takes the
            -- difference
            local sum = 0
            for _, r in ipairs(V.resources) do
                local old = mem.target[r] or 0
                target[r] = old + clamp(target[r] - old, -3, 3)
                sum = sum + target[r]
            end
            target.food = math.max(target.food + view.citizens - sum, 0)
            -- the mines' places can outnumber the citizens from the Copper Age (seed 9: wood 16, stone 12, gold 19,
            -- iron 30 for 70 citizens left food at 0 for 40 minutes): food keeps its floor, the largest other gives
            local floor_late = math.min(view.citizens // 3, math.max((mem.fields or 0) + 2, 6))
            while target.food < floor_late do
                local from = nil
                for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
                    if target[r] > 2 and (from == nil or target[r] > target[from]) then
                        from = r
                    end
                end
                if from == nil then
                    break
                end
                target[from] = target[from] - 1
                target.food = target.food + 1
            end
        end
        if target ~= nil then
            mem.target = target
            local hunt = claims(view, mem, target, order)
            hunt = hunt or (view.time - (mem.no_drop_t or -100000) < 30000 and view.hunt > 0)
            -- the patches blocked or bare (the food gatherers at work far under the target and the income low): the
            -- game near home (food_plan HUNT)
            hunt = hunt or (view.hunt > 0 and view.gatherers.food * 2 < target.food and view.income.food < 90)
            -- citizens idle for want of a food site (an island with the patches bare): the game, more boats, the claim
            if view.idle >= 5 and view.gatherers.food < target.food then
                mem.idle_t = mem.idle_t or view.time
                if view.time - mem.idle_t >= 60000 then
                    hunt = hunt or view.hunt > 0
                    mem.more_boats = math.min((mem.more_boats or 0) + 2, 12)
                    if view.claim_range < view.claim_range_max and view.time - (mem.range_t or 0) >= 60000 then
                        order({verb = "claim_range", range = view.claim_range + view.claim_range_step})
                        mem.range_t = view.time
                    end
                    mem.idle_t = view.time
                end
            else
                mem.idle_t = nil
            end
            local prefer = hunt and "hunt" or "patch"
            for _, r in ipairs(V.resources) do
                if target[r] ~= view.wanted[r] or (r == "food" and prefer ~= (mem.prefer or "patch")) then
                    if r == "food" then
                        order({verb = "gather", resource = r, n = target[r], prefer = prefer})
                        mem.prefer = prefer
                    else
                        order({verb = "gather", resource = r, n = target[r]})
                    end
                end
            end
            settlements(view, mem, target, order)
            -- [priority.c] the buildings spend what the list leaves a building (the silos' wood held from the ones
            -- that are not food, houses, towns or docks); the engine's reserve holds the epoch's and the towns'
            local above = build_vis
            mem.tower_stone = towers(view, mem, order, above)
            production(view, mem, order, army, above)
            mem.fields = farms(view, mem, order, target, build_econ_vis)
            if saving then
                sol(view, mem, order)
            end
            -- [navy.c] docks and fishing boats are the navy's (ageofai.navy: navy_docks, the fishing boat type and
            -- count; mem.more_boats is read there)
        end
    end
    research(view, mem, order)
    citizens_home(view, mem)   -- [econ.c] the idle citizens away from the towns
    -- [priority.c] hold: a military unit's hold (metal.lua's SPARE prices the stock above it)
    return {reserve = reserve, avail = avail, civil = civil, saving = saving, target = target, citizens_wanted = want_citizens,
            order = order_vis, ship = ship_vis, build = build_vis, build_econ = build_econ_vis, hold = hold_mil}
end

return M

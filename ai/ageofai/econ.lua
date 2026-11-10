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
local M = {}

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
    if view.epoch < view.last_epoch and not view.advancing then
        local age = view.time - view.epoch_start
        if view.epoch < FARM_EPOCH then
            -- the copper plan: the economy first, then the saving; the Stone Age grows to stone_workforce first (a
            -- deviation: upstream holds both epochs' costs from min_workforce on, which its own measurements call
            -- the most harmful brake; the owner's build order grows to about 64 in the Stone Age)
            local at = view.epoch == 0 and S.min_workforce or S.stone_workforce
            saving = view.citizens >= at or age >= 18 * 60000
            mem.peak = math.max(mem.peak or 0, view.citizens)
            if view.citizens * 100 < (mem.peak or 0) * S.recover_share and view.citizens < at then
                saving = false   -- the economy lost citizens: it grows again before it saves (recover_economy)
            end
            if view.income.food < 60 and view.citizens < at and view.stock.food < view.next_epoch_cost.food then
                saving = false   -- no food coming in: saving starves the town (the raided patches); the citizens first
            end
        elseif S.epoch_pacing == "rush" then
            saving = age >= 5 * 60000
        else
            local at = (mem.epoch_at or 0)
            saving = view.time >= at - S.reserve_window_min * 60000
        end
    end
    for _, k in ipairs(V.resources) do
        r[k] = saving and view.next_epoch_cost[k] or 0
    end
    return r, saving
end

-- the military account: a share of the start stock, then of every income; what it may spend now per resource
-- (within the stock above the reserve), the hoard above reserve + cap free for the army too
function M.accounts(view, mem, reserve)
    mem.mil = mem.mil or {}
    local mil = mem.mil
    if not mem.mil_init then
        mem.mil_init = true
        for _, r in ipairs(V.resources) do
            mil[r] = view.stock[r] * S.military_start_share // 100
        end
    end
    local share = view.epoch >= FARM_EPOCH and S.military_income_share_farms or S.military_income_share
    if S.early_army and view.epoch == 0 then
        share = 0
    end
    -- the income over the last 60 s, one second's worth a step
    local dt = math.max(view.time - (mem.mil_t or view.time), 0)
    mem.mil_t = view.time
    if dt > 0 and dt <= 5000 then
        for _, r in ipairs(V.resources) do
            mil[r] = (mil[r] or 0) + view.income[r] * dt * share // (60000 * 100)
        end
    end
    local avail, civil = {}, {}
    for _, r in ipairs(V.resources) do
        local above = math.max(view.stock[r] - reserve[r], 0)
        local cap = (r == "food" or r == "wood") and S.hoard_cap or S.hoard_cap_mineral
        mil[r] = math.min(mil[r] or 0, above + cap)   -- the account never outgrows what could be spent
        local free = math.max(above - cap, 0)
        avail[r] = math.min(above, (mil[r] or 0) + free)
        civil[r] = above
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
    local floor_food = late and math.min(total // 2, (wants.fields or 0) + 2)
        or math.min(total * S.food_share // 100, S.food_floor)
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

local function nearest_free(view, resource, patch, range)
    local best = nil
    for _, s in ipairs(view.sites) do
        if not (s.claimed or s.resource ~= resource or s.units == 0 or s.danger > 0 or (patch and not s.patch)
                or s.distance > range or s.blocked == "tower" or s.blocked == "enemy_near") then
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
    local free_patch = nearest_free(view, "food", true, range)
    -- forage patches with room for the food gatherers (two at least, four at most); the claim grows when none is
    -- left in it; the food gatherers hunt when no patch is left at all (the game near home first: food_plan HUNT)
    local most = view.epoch == 0 and 3 or 5
    if patches < 2 or (patches < most and patch_room < target.food - view.gatherers.food + 2) then
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
                local site = nearest_free(view, r, false, range)
                if site then
                    order({verb = "claim", site = site.index})
                elseif range < view.claim_range_max and n == 0 then
                    order({verb = "claim_range", range = range + view.claim_range_step})
                end
            end
        end
    end
    -- a claimed site with nothing left, or blocked for good, is released
    for _, s in ipairs(view.sites) do
        if s.claimed and (s.units == 0 or s.blocked == "tower") then
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
-- game); one such building at a time, three minutes at most. Returns true when ordered
local function build_direct(view, mem, type)
    mem.direct = mem.direct or {}
    local d = mem.direct
    if d.id ~= nil then
        local u = ai.unit(d.id)
        local done = u == nil or u.built or view.time - d.t > 180000
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
    local gx, gy
    for _, opt in ipairs({{min = 4, max = 20, safe = true}, {min = 4, max = 20}, {min = 2, max = 30}}) do
        for r = opt.min, opt.max, 3 do
            local x, y = ai.find_site(type, view.town_x, view.town_y, {min = r, max = math.min(r + 2, opt.max), safe = opt.safe})
            if x ~= nil and ai.reach(view.town_x, view.town_y, x, y)
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
        d.id, d.t, d.builders, d.type = id, view.time, cits, type
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
    for _, s in ipairs(view.sites) do
        if s.claimed and s.units > 0 and s.danger == 0 and s.blocked ~= "tower" and s.blocked ~= "enemy_near" then
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
    if above.stone < cost.cost.stone + S.tower_stone_keep then
        return stone_wanted + cost.cost.stone
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
    local x, y = front_spot(view, mem, tower, fx, fy, len)
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
            if not going and V.total(have) < want[t] and ok then
                if (mem.nosite or {})[t] ~= nil and view.time - mem.nosite[t] < 600000 then
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
local function farms(view, mem, order, target, above)
    if view.epoch < FARM_EPOCH then
        return 0
    end
    local gran = V.buildings(view, "b_granary")
    local farm = V.buildings(view, "b_farm")
    local fields = V.total(farm)
    local granaries = V.total(gran)
    local want_gran = math.min(1 + (target.food or 0) // 8, 8)
    if V.cost(view, "b_granary") ~= nil and granaries < want_gran and (gran == nil or gran.unfinished + gran.queued == 0)
            and (view.gatherers.food + 6 <= (target.food or 0) or granaries == 0) and above.wood >= 100 then
        -- the town's rings full (an island town: the verb found no site near the town nor round a tile): the
        -- building placed through the per-unit interface at a spot of its own
        if (mem.nosite or {}).b_granary ~= nil and view.time - mem.nosite.b_granary < 600000 then
            -- OFF (S.direct_granary): a granary placed through the human's build message lays its field markers;
            -- where one of them cannot be placed the granary stays pending and a builder's BeginRepair on it
            -- crashes the game (EEStepAsideFromSite: a Small Islands game at 49 min, twice) - an engine guard first
            if S.direct_granary and view.time - (mem.direct_t or 0) >= 30000 then
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
    -- OFF (S.replant_fields): the replant's field markers were removed again and a builder's BeginRepair on one
    -- crashed the game (EEStepAsideFromSite; a Small Islands game at 49 min) - the message runs outside its safe point
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
    if best ~= nil and not building then
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
        -- the copper plan caps the workforce at min_workforce before the saving; the Stone Age grows further (the
        -- owner's build order: about 32 in the Prehistoric, 64 in the Stone Age)
        return math.min(game, view.epoch == 0 and S.min_workforce or S.min_workforce + 30)
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
    mem.orders_ref = orders
    local function order(o)
        orders[#orders + 1] = o
        return #orders
    end
    local reserve, saving = M.reserve(view, mem)
    local avail, civil = M.accounts(view, mem, reserve)
    -- the engine's reserve: the epoch's; the military account is Lua's own (the verbs see the stock above the reserve)
    local differs = false
    for _, r in ipairs(V.resources) do
        differs = differs or reserve[r] ~= view.reserve[r]
    end
    if differs then
        order({verb = "reserve", food = reserve.food, wood = reserve.wood, stone = reserve.stone, gold = reserve.gold,
               iron = reserve.iron})
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
        local timer_ok = S.epoch_pacing == "rush" or view.time >= (mem.epoch_at or 0)
        if paid and timer_ok then
            order({verb = "advance"})
        end
    end

    -- citizens: two at a time from each town centre while the civil budget keeps a citizen's food
    local citizen = V.cost(view, "citizen")
    local want_citizens = citizens_wanted(view, mem.target)
    local making = citizen and citizen.in_production or 0
    local waiting = citizen and citizen.requested or 0
    local planned = view.citizens + making + waiting
    if citizen and waiting == 0 and making < 2 * citizen.producers and planned < want_citizens
            and view.population < view.population_cap and not view.advancing and civil.food >= citizen.cost.food then
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
        local wants = {citizens = want_citizens, tower_stone = mem.tower_stone or 0, fields = mem.fields or 0}
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
            local above = {}
            for _, r in ipairs(V.resources) do
                above[r] = math.max(view.stock[r] - reserve[r], 0)   -- the buildings spend the stock above the reserve
            end
            mem.tower_stone = towers(view, mem, order, above)
            production(view, mem, order, army, above)
            mem.fields = farms(view, mem, order, target, above)
            if saving then
                sol(view, mem, order)
            end
            -- fishing from the Stone Age
            local age = math.min(view.epoch, 2) + 1
            if (S.boats[age] or 0) > 0 then
                order({verb = "fish", boats = S.boats[age] + (mem.more_boats or 0), docks = S.docks[age]})
            end
        end
    end
    research(view, mem, order)
    return {reserve = reserve, avail = avail, civil = civil, saving = saving, target = target, citizens_wanted = want_citizens}
end

return M

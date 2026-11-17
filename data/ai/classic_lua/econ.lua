-- EECPResourceManager::Update in Lua (the classic port; ORIGIN in this folder): the wanted gatherers every 5 s (the
-- original's needs split with the personality's stock correction, or the demand split: economy.demand_split, with
-- the claimed centres, the patch settlements and the site towers of economy.forage_patches), the assignment every
-- 10 s (RebalanceGatherers, then the idle citizens by resource: food by hunt, bushes, farms, fish), the resource
-- check every 15 s. require("classic_lua.econ").
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local C = T.C

local M = {}

local RES = R.RES

local function U()
    return require("classic_lua.units")
end

local function B()
    return require("classic_lua.build")
end

local function F()
    return require("classic_lua.ferry")
end

-- ---------------------------------------------------------------------------------------------------------------
-- GetGatherCapacity(r): what the centres' units hold (the active centres; all when none is): the mining limit a unit,
-- wood the personality's wood_slots; food also what works farms, fish and game (forage_patches: the game near home
-- up to 12), and the coast boats to come. nil: the sum

local HUNT_RANGE, HUNT_SLOTS = 45, 12

-- the food gatherers outside the centres: boats, farmers, hunters; the game within 45 tiles (EECPRMFoodOutsideCenters)
local function food_outside(view, memory)
    local rm = R.state(memory)
    local farm = memory.civ ~= nil and memory.civ.farm_type or C.farm
    local count, hunters = 0, 0
    for _, id in ipairs(rm.lists.food) do
        local u = R.unit(id)
        if u ~= nil then
            if u.category == "boat" then
                count = count + 1
            elseif u.target ~= nil then
                local t = R.unit(u.target)
                if t ~= nil and t.type == farm then
                    count = count + 1
                elseif t ~= nil and t.type ~= C.forage_bush and t.type ~= C.fish then
                    hunters = hunters + 1
                end
            end
        end
    end
    local game = 0
    local tx, ty = R.town(view)
    if tx ~= nil then
        for _, id in ipairs(rm.mines) do
            local u = R.unit(id)
            if u ~= nil and (u.amount or 0) > 0 and R.d2(u.x, u.y, tx, ty) <= HUNT_RANGE * HUNT_RANGE then
                game = game + 1
            end
        end
    end
    if game > HUNT_SLOTS then
        game = HUNT_SLOTS
    end
    return count + math.max(hunters, game)
end

-- the coast's boats wanted (EECPRMCoastBoatsWanted): from the Stone Age, 2 a fish within 30 tiles of the town, 8 at
-- most (32 with the forage patches)
function M.coast_boats_wanted(view, memory)
    return R.memo("coast_boats", function()
        return M.coast_boats_wanted_now(view, memory)
    end)
end

function M.coast_boats_wanted_now(view, memory)
    local rm = R.state(memory)
    local p = memory.t.personality
    if rm.uses_fishing or not p.coast_fishing or view.epoch < 1 then
        return 0
    end
    local tx, ty = R.town(view)
    if tx == nil then
        return 0
    end
    -- New (AI_RESEARCH 8.97): on island and planets maps the fish within 30 tiles of a built dock count too (a second
    -- dock at fresh fishing grounds, island_dock below)
    local docks = {}
    if F().water_map(memory) then
        for _, id in ipairs(ai.units{type = C.dock, built = true}) do
            local d = R.unit(id)
            if d ~= nil then
                docks[#docks + 1] = d
            end
        end
    end
    local n = 0
    for _, id in ipairs(rm.fish) do
        local u = R.unit(id)
        if u ~= nil and (u.amount or 0) > 0 then
            local ok = R.d2(u.x, u.y, tx, ty) <= 900
            for k = 1, #docks do
                if ok then
                    break
                end
                ok = R.d2(u.x, u.y, docks[k].x, docks[k].y) <= 900
            end
            if ok then
                n = n + 1
            end
        end
    end
    local max = p.forage_patches and 32 or 8
    n = n * 2
    return n < max and n or max
end

local function fishes(view, memory)
    return R.state(memory).uses_fishing or M.coast_boats_wanted(view, memory) > 0
end

-- untapped: count the untapped sites only (a resource's hit points fall from 1 when it is first gathered from), as
-- the classic's GetGatherCapacity does for the citizen target (GetWantedCitizens); a resource's own room (the demand
-- split, an advance's reachable resources) counts every site with something left, as the classic since its fix
-- (AI_RESEARCH 8.58, 8.63: counting only the untapped sites there starved a small island's food)
function M.gather_capacity(view, memory, which, untapped)
    return R.memo("capacity_" .. tostring(which) .. (untapped and "_u" or ""), function()
        return M.gather_capacity_now(view, memory, which, untapped)
    end)
end

function M.gather_capacity_now(view, memory, which, untapped)
    local rm = R.state(memory)
    local sites = {food = 0, wood = 0, stone = 0, gold = 0, iron = 0}
    local active = 0
    for pass = 1, 2 do
        if active > 0 then
            break
        end
        for _, c in ipairs(rm.centres) do
            if pass == 2 or c.active then
                active = active + 1
                for _, id in ipairs(c.u) do
                    -- (the classic counts a centre's units with mHitPoints > 0: a resource's hit points fall from 1
                    -- when it is first gathered from, so only the untapped ones count; the record's `untapped`)
                    -- New (AI_RESEARCH 8.88): not the sites with an enemy near (FindResourceTarget sends no one
                    -- there): on a small map half the claimed gold lay by the enemy's town, the wanted gold was twice
                    -- what could be placed, and the rest of the citizens went to wood
                    local u = R.unit(id)
                    if u ~= nil and u.kind ~= nil and (not untapped or u.untapped ~= false)
                        and not R.enemy_near(u.x, u.y) then
                        sites[u.kind] = sites[u.kind] + 1
                    end
                end
            end
        end
    end
    local limit = T.b(memory, "mining_limit")
    if limit <= 0 then
        limit = 1
    end
    local total = 0
    local p = memory.t.personality
    for _, r in ipairs(RES) do
        local cap = r == "wood" and p.wood_slots or sites[r] * limit
        if r == "food" then
            if p.forage_patches then
                cap = cap + food_outside(view, memory)
            else
                cap = cap + #rm.lists.food
            end
            local boats = M.coast_boats_wanted(view, memory)
            if boats > 0 then
                local have = #ai.units{type = T.fishing_boat_class(view.epoch)}
                if boats > have then
                    cap = cap + boats - have
                end
            end
        end
        if r == which then
            return cap
        end
        total = total + cap
    end
    return total
end

-- ---------------------------------------------------------------------------------------------------------------
-- the boats (EECPRMRequestBoats / RequestFishingBoats)

local function request_boats(view, memory)
    local wanted = M.coast_boats_wanted(view, memory)
    local boat = T.fishing_boat_class(view.epoch)
    if wanted == 0 then
        -- RequestFishingBoats(1): unless one exists or is requested
        if #ai.units{type = boat} == 0 and not U().is_requested(memory, boat) and not U().in_production(view, boat) then
            U().request(view, memory, boat, 1)
        end
        return
    end
    local built = #ai.units{type = C.dock, built = true}
    if built == 0 then
        B().queue(view, memory, C.dock, false, true)
        return
    end
    -- (EECPRMRequestBoats' count: the boats it has, requested or being made; fixed in the C++ too, AI_RESEARCH 8.56)
    local um = U().state(memory)
    if (um.waiting[boat] or 0) == 0 and not U().in_production(view, boat) and #ai.units{type = boat} < wanted then
        U().request(view, memory, boat, 1)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the forage patches' rules (economy.forage_patches): the patch settlements, the site towers, the centres claimed

local SETTLE_MS, GUARD_MS = 20000, 30000

-- EECPRMSettlePatches: a settlement by the forage patch (and the worked mines) of an active centre in the claim range
-- with no drop site within 3 tiles, nearest the town first, one every 20 s
local function settle_patches(view, memory)
    local rm = R.state(memory)
    if not rm.settle_enabled then
        return
    end
    local t = U().class_tech(view, C.settlement)
    if t == nil or not t.producible then
        return
    end
    if rm.settle_last ~= nil and view.time - rm.settle_last < SETTLE_MS then
        return
    end
    if B().is_queued(memory, C.settlement) or U().has_unfinished(C.settlement) then
        return
    end
    local tx, ty = R.town(view)
    if tx == nil then
        return
    end
    local best, bd
    for _, c in ipairs(rm.centres) do
        if c.active then
            for _, r in ipairs({"food", "stone", "gold", "iron"}) do
                local u = R.find_resource_unit(c, r)
                if u ~= nil and (r ~= "food" or u.type == C.forage_bush) and (r == "food" or R.gatherers(u.id) > 0)
                    and R.in_claim(view, memory, u.x, u.y) and not R.drop_near(u.x, u.y, 3, r == "food")
                    and not R.enemy_near(u.x, u.y) and not R.danger_at(memory, u.x, u.y) then
                    local dd = R.d2(u.x, u.y, tx, ty)
                    if bd == nil or dd < bd then
                        best, bd = u, dd
                    end
                end
            end
        end
    end
    if best == nil then
        return
    end
    -- EECPRMPatchSite: rings 2 .. 5 round the patch, with a margin
    local x, y = R.find_site(C.settlement, best.x, best.y, {min = 2, max = 5, margin = true})
    if x == nil then
        R.claim_expand(view, memory)
        return
    end
    rm.settle_last = view.time
    B().queue(view, memory, C.settlement, false, false, x, y)
end

-- EECPRMGuardSites: a tower for each worked site of an active centre in the claim range with none within 8 tiles, one
-- every 30 s
local function guard_sites(view, memory)
    local rm = R.state(memory)
    if rm.guard_last ~= nil and view.time - rm.guard_last < GUARD_MS then
        return
    end
    local tower = T.tower_class(view.epoch)
    local t = U().class_tech(view, tower)
    if t == nil or not t.producible or B().is_queued(memory, tower) or U().has_unfinished(tower) then
        return
    end
    for _, c in ipairs(rm.centres) do
        if c.active then
            for _, r in ipairs(RES) do
                local u = R.find_resource_unit(c, r)
                if u ~= nil and R.gatherers(u.id) > 0 and R.in_claim(view, memory, u.x, u.y)
                    and not B().tower_near(view, u.x, u.y, 64) then
                    rm.guard_last = view.time
                    B().queue(view, memory, tower, false, true)
                    return
                end
            end
        end
    end
end

-- BuildSettlementFor (EEUGGather::FindStorage asks it when a computer player's gatherer looks for a drop site: the
-- engine hook skips a script's seat, so the port looks every 4 s at the gatherers at work): a gatherer whose
-- resource has no drop site within the drop-off distance builds a settlement by it, with settlements enabled and
-- producible, none ordered for that resource yet, not for game, a forest of 5 trees at least, the site within the
-- drop-off distance of the resource and no town centre within 3/4 of it. DEVIATION: the gatherer's own storage is
-- the nearest drop site (any drop within the distance counts); the site is the rings 2 .. the distance round the
-- resource (the centre's FindBuildSite and FindWoodSite are not ported apart)
local SETTLE_FOR_MS, SETTLE_FOR_STALE = 4000, 120000
local function settlement_for(view, memory)
    local rm = R.state(memory)
    if not rm.settle_enabled or view.time < (rm.settle_for_next or 0) then
        return
    end
    rm.settle_for_next = view.time + SETTLE_FOR_MS
    rm.settle_for = rm.settle_for or {}
    -- IsSettlementQueued: an order stands until a drop site stands there or it went stale
    for _, k in ipairs(ai.keys(rm.settle_for)) do
        local o = rm.settle_for[k]
        if view.time - o.t > SETTLE_FOR_STALE or R.drop_near(o.x, o.y, 1, false) then
            rm.settle_for[k] = nil
        end
    end
    local t = U().class_tech(view, C.settlement)
    if t == nil or not t.producible or not U().can_afford(view, memory, U().cost(C.settlement), 1, C.settlement) then
        return
    end
    local r = T.b(memory, 'drop_off_distance')
    local ids = ai.units{category = 'citizen', held = 'lua'}
    if #ids == 0 then
        return
    end
    local cols = ai.get(ids, {'gathers', 'target'})
    local tried = {}   -- (a resource tested once a call: its gatherers share the answer; the late game's 120 gatherers
                       -- tested each blew the step's budget)
    for k = 1, #ids do
        local kind, target = cols.gathers[k], cols.target[k]
        if kind ~= nil and target ~= nil and rm.settle_for[kind] == nil and not tried[target] then
            tried[target] = true
            local res = R.unit(target)
            local c = res ~= nil and R.class_of(res.type) or nil
            if res ~= nil and c.family ~= 0x11 and res.type ~= C.farm and res.type ~= C.robotic_farm
                    and not R.drop_near(res.x, res.y, r, kind == 'food') then
                local ok = true
                if kind == 'wood' then
                    ok = #ai.resources{kind = 'wood', near = {res.x, res.y, 3}, limit = 5} >= 5
                end
                if ok then
                    local x, y = R.find_site(C.settlement, res.x, res.y, {min = 2, max = r, margin = true})
                    if x ~= nil and R.d2(x, y, res.x, res.y) <= r * r then
                        local near = false
                        for _, d in ipairs(R.drops(false)) do
                            if R.d2(d.x, d.y, x, y) * 16 <= r * r * 9 then
                                near = true
                            end
                        end
                        if not near then
                            local code = ai.build({ids[k]}, C.settlement, x, y)
                            if code == 0 or code == true or code == 'ok' then
                                rm.settle_for[kind] = {x = x, y = y, t = view.time}
                                print(string.format('classic_lua settlement.order resource=%s x=%d y=%d', kind, x, y))
                                return
                            end
                        end
                    end
                end
            end
        end
    end
end

-- EECPRMActivateNeeded: the forage patches kept claimed (3 in the Prehistoric, 5 later), the patch settlements and
-- site towers, the claim range grown before the farms when no patch is left; economy.claim_needed: a centre with the
-- next epoch's missing stone, gold or iron claimed
local function activate_needed(view, memory)
    local rm = R.state(memory)
    local p = memory.t.personality
    local tx, ty = R.town(view)
    if tx == nil then
        return
    end
    if p.forage_patches then
        local patches = 0
        for _, c in ipairs(rm.centres) do
            if c.active then
                for _, id in ipairs(c.u) do
                    local u = R.unit(id)
                    if u ~= nil and u.type == C.forage_bush and (u.amount or 0) > 0 then
                        patches = patches + 1
                    end
                end
            end
        end
        local wanted = view.epoch <= 0 and 3 or 5
        if p.claim_range then
            settle_patches(view, memory)
            guard_sites(view, memory)
        end
        if patches < wanted then
            local best, bd
            for _, c in ipairs(rm.centres) do
                if not c.active then
                    local u = R.find_resource_unit(c, "food")
                    if u ~= nil and u.type == C.forage_bush and not R.danger_at(memory, u.x, u.y)
                        and (not p.claim_range or R.in_claim(view, memory, u.x, u.y)) then
                        local dd = R.d2(u.x, u.y, tx, ty)
                        if bd == nil or dd < bd then
                            best, bd = c, dd
                        end
                    end
                end
            end
            if best ~= nil then
                best.active = true
            elseif p.patch_expand and p.claim_range and patches == 0 and view.epoch < 2 then
                R.claim_expand(view, memory)
            end
        end
    end
    if not p.claim_needed then
        return
    end
    local e = ai.epoch(view.epoch + 1)
    if e == nil then
        return
    end
    for _, r in ipairs({"stone", "gold", "iron"}) do
        if (e.cost[r] or 0) > view.stock[r] and M.gather_capacity(view, memory, r) <= 0 then
            local best, bd
            for _, c in ipairs(rm.centres) do
                if not c.active then
                    local u = R.find_resource_unit(c, r)
                    if u ~= nil and not R.danger_at(memory, u.x, u.y)
                        and (not p.claim_range or R.in_claim(view, memory, u.x, u.y)) then
                        local dd = R.d2(u.x, u.y, tx, ty)
                        if bd == nil or dd < bd then
                            best, bd = c, dd
                        end
                    end
                end
            end
            if best ~= nil then
                best.active = true
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the splits

-- EECPRMGetDemand: tier 1 the next citizens (15 at most), the standing army's shortfall, the first two buildings of
-- each build queue; tier 2 the pending epoch by its pressure; tier 3 the other unit requests, the research, the rest
-- of the build queues
local function demand(view, memory)
    local tiers = {{}, {}, {}}
    for t = 1, 3 do
        for _, r in ipairs(RES) do
            tiers[t][r] = 0
        end
    end
    local function add(tier, cost, n)
        if cost == nil or n <= 0 then
            return
        end
        for _, r in ipairs(RES) do
            tiers[tier][r] = tiers[tier][r] + (cost[r] or 0) * n
        end
    end
    local um = U().state(memory)
    local missing = U().wanted_citizens(view, memory) - U().workers(view, memory)
    if missing > 15 then
        missing = 15
    end
    add(1, U().cost(C.citizen), missing)
    local fav = memory.cp.favorite_land
    if fav ~= nil then
        local target = memory.t.personality.standing_army_k * U().workers(view, memory) // 1000
        local short = target - U().military(view, memory, false)
        if short > 15 then
            short = 15
        end
        add(1, U().cost(fav), short)
    end
    for e = 1, 15 do
        for _, q in ipairs(um.req[e]) do
            if q.class ~= C.citizen then
                add(3, U().cost(q.class), q.n)
            end
        end
    end
    local bm = B().state(memory)
    for e = 1, 15 do
        for k, q in ipairs(bm.q[e]) do
            add(k <= 2 and 1 or 3, U().cost(q.class), 1)
        end
    end
    local rs = memory.rs
    if rs ~= nil then
        local tree = U().tree(view)
        for e = 1, 15 do
            for _, id in ipairs(rs.ids[e]) do
                if id >= 15 then
                    local t = tree.byId[id]
                    if t ~= nil then
                        add(3, t.cost, 1)
                    end
                end
            end
        end
    end
    local share, reserved = U().reserve_share(view, memory)
    if reserved ~= nil and share > 0 then
        for _, r in ipairs(RES) do
            tiers[2][r] = tiers[2][r] + (reserved[r] or 0) * share // 1000
        end
    end
    return tiers
end

-- EECPRMSplitByDemand: half the personality's floors first, the rest by the shortfall (demand less stock) plus a
-- quarter of the demand, capped by the capacity; what the caps leave goes to the heaviest with room
local function split_by_demand(view, memory)
    local rm = R.state(memory)
    activate_needed(view, memory)
    local gatherers = R.total_gatherers(rm)
    local tiers = demand(view, memory)
    local nofood = view.epoch > 13
    local weight, total = {}, 0
    for _, r in ipairs(RES) do
        weight[r] = 0
        if not (r == "food" and nofood) then
            local want = tiers[1][r] + tiers[2][r] + tiers[3][r] // 2
            local short = want - view.stock[r]
            weight[r] = (short > 0 and short or 0) + want // 4
            total = total + weight[r]
        end
    end
    local wanted = {food = 0, wood = 0, stone = 0, gold = 0, iron = 0}
    if gatherers > 0 then
        local floor = memory.t.personality.floor_k
        local floorsum = 0
        for _, r in ipairs(RES) do
            if not (r == "food" and nofood) then
                floorsum = floorsum + floor[r] // 2
            end
        end
        if floorsum > 1000 then
            floorsum = 1000
        end
        local room, left = {}, gatherers
        for _, r in ipairs(RES) do
            room[r] = M.gather_capacity(view, memory, r)
            local base = (r == "food" and nofood) and 0 or floor[r] // 2
            local part = total > 0 and (1000 - floorsum) * weight[r] // total or 0
            local share = ((base + part) * gatherers + 500) // 1000
            if share > room[r] then
                share = room[r]
            end
            if share == 0 and weight[r] > 0 and room[r] > 0 then
                share = 1
            end
            wanted[r] = share
            left = left - share
        end
        while left > 0 do
            local best
            for _, r in ipairs(RES) do
                if weight[r] > 0 and wanted[r] < room[r]
                    and (best == nil or weight[r] * (wanted[best] + 1) > weight[best] * (wanted[r] + 1)) then
                    best = r
                end
            end
            if best == nil then
                break
            end
            wanted[best] = wanted[best] + 1
            left = left - 1
        end
    end
    rm.wanted = wanted
    -- New (AI_RESEARCH 8.99): the gatherers past every resource's room (60-90 of 150-180 citizens on wood, a wood
    -- stock of 5,000-11,000) go to farms: with 10 or more spare and food at its room, one more granary (whose
    -- foundations are the farms) a minute, beyond the building counts' granary_max. Food income +13% (64 seeds)
    if gatherers > 0 and view.epoch >= 2 and not rm.uses_fishing then
        local placed = 0
        for _, r in ipairs(RES) do
            placed = placed + wanted[r]
        end
        if gatherers - placed >= 10 and weight.food > 0 and wanted.food >= M.gather_capacity(view, memory, "food")
                and not B().is_queued(memory, C.granary) and not U().has_unfinished(C.granary)
                and view.time - (rm.granary_more_t or -60000) >= 60000 then
            rm.granary_more_t = view.time
            B().queue(view, memory, C.granary, false, false)
            print(string.format("classic_lua granary.more spare=%d", gatherers - placed))
        end
    end
    if view.time % 30000 < 5000 then
        local function row(t)
            return string.format("%d/%d/%d/%d/%d", t.food, t.wood, t.stone, t.gold, t.iron)
        end
        print(string.format("classic_lua demand t1=%s t2=%s t3=%s wanted=%s of=%d", row(tiers[1]), row(tiers[2]), row(tiers[3]),
            row(wanted), gatherers))
    end
end

-- UpdateWantedGatherers (economy.demand_split false: the original's needs ledger): the gatherers by the needs' shares
-- (the lowest need lifted to 0), the minerals capped by their centres' units x the mining limit, at least 1 while a
-- need stands; then the personality's stock correction (surplus_cut over surplus, the floor under low_stock with
-- the standing army's floor)
local function update_wanted(view, memory)
    if memory.t.personality.demand_split then
        return split_by_demand(view, memory)
    end
    local rm = R.state(memory)
    local limit = T.b(memory, "mining_limit")
    local needs = rm.needs
    local lowest = 100000
    for _, r in ipairs(RES) do
        if needs[r] <= lowest then
            lowest = needs[r]
        end
    end
    if lowest < 0 then
        for _, r in ipairs(RES) do
            if needs[r] ~= 0 then
                needs[r] = needs[r] - lowest
            end
        end
    end
    local gatherers, total = #rm.lists.idle, 0
    for _, r in ipairs(RES) do
        gatherers = gatherers + #rm.lists[r]
        total = total + needs[r]
    end
    if gatherers > 0 and total > 0 then
        for _, r in ipairs(RES) do
            local need = needs[r]
            if need >= 0 then
                local w = (need * gatherers * 2 + total) // (2 * total)   -- UMFloatToLong: round to nearest
                if r == "food" then
                    if view.epoch > 13 then
                        w, need = 0, 0
                    end
                elseif r ~= "wood" then
                    local found = 0
                    for _, c in ipairs(rm.centres) do
                        for _, id in ipairs(c.u) do
                            local u = R.unit(id)
                            if u ~= nil and u.kind == r then
                                found = found + 1
                            end
                        end
                    end
                    if found * limit < w then
                        w = found * limit
                    end
                end
                if need > 0 and w <= 1 then
                    w = 1
                end
                rm.wanted[r] = w
            else
                rm.wanted[r] = 0
            end
        end
    end
    -- the stock correction with the standing army's floor
    local p = memory.t.personality
    if gatherers > 0 then
        local armyfloor = {food = 0, wood = 0, stone = 0, gold = 0, iron = 0}
        local fav = memory.cp.favorite_land
        if fav ~= nil and (p.army_floor_k or 0) > 0 then
            local target = p.standing_army_k * U().workers(view, memory) // 1000
            local army = U().military(view, memory, false)
            if target > 0 and army < target then
                local cost = U().cost(fav)
                local sum = 0
                for _, r in ipairs(RES) do
                    sum = sum + (cost[r] or 0)
                end
                if sum > 0 then
                    for _, r in ipairs(RES) do
                        armyfloor[r] = p.army_floor_k * (target - army) * (cost[r] or 0) // (target * sum)
                    end
                end
            end
        end
        for _, r in ipairs(RES) do
            local w = rm.wanted[r]
            local stock = view.stock[r]
            if stock > p.surplus then
                w = (w * p.surplus_cut_k + 500) // 1000
            elseif stock < p.low_stock then
                local floor = ((p.floor_k[r] + armyfloor[r]) * gatherers + 500) // 1000
                if r == "food" and view.epoch > 13 then
                    floor = 0
                end
                if w < floor then
                    w = floor
                end
            end
            rm.wanted[r] = w
        end
    end
end
M.update_wanted = update_wanted

-- ---------------------------------------------------------------------------------------------------------------
-- RebalanceGatherers: the builders done with their building freed; up to moves_per_cycle gatherers of a resource
-- more than one over its wanted count freed while another resource is short

local function rebalance(view, memory, records)
    local rm = R.state(memory)
    for id, u in pairs(records) do
        if u.action == "build" and u.target ~= nil then
            local b = R.unit(u.target)
            if b == nil or (b.built and (b.hp or 0) >= (b.hp_max or 0)) then
                ai.stop({id})
                R.add_idle(rm, id)
                u.action = "idle"
            end
        end
    end
    local short = false
    for _, r in ipairs(RES) do
        if #rm.lists[r] < rm.wanted[r] then
            short = true
        end
    end
    if not short then
        return
    end
    local moves, max = 0, memory.t.personality.moves_per_cycle or 0
    for _, r in ipairs(RES) do
        while moves < max and #rm.lists[r] > rm.wanted[r] + 1 do
            local id = rm.lists[r][#rm.lists[r]]
            local u = records[id]
            if u == nil or u.category == "boat" or u.action == "build" then
                break
            end
            ai.stop({id})
            R.add_idle(rm, id)
            u.action = "idle"
            moves = moves + 1
        end
    end
end

-- AssignIdleGatherers: each idle citizen to the most under-served of gold, iron, stone, wood (the C++ port's order)
local IDLE_A_PASS = 12   -- the idle citizens placed a pass (every 10 s; the rest at the next: the instruction budget)

local function assign_idle_gatherers(view, memory, idle)
    local rm = R.state(memory)
    for k, u in ipairs(idle) do
        if k > IDLE_A_PASS then
            break
        end
        local order = {"gold", "iron", "stone", "wood"}
        table.sort(order, function(a, b)
            local da, db = rm.wanted[a] - #rm.lists[a], rm.wanted[b] - #rm.lists[b]
            if da ~= db then
                return da > db
            end
            local rank = {gold = 1, iron = 2, stone = 3, wood = 4}
            return rank[a] < rank[b]
        end)
        local sent = false
        for _, r in ipairs(order) do
            local res = r == "wood" and R.find_forest_target(view, memory, 1, u)
                or R.find_resource_target(view, memory, r, u, false)
            if res ~= nil then
                R.send_to_gather(memory, u.id, res)
                sent = true
                break
            end
        end
        -- DEVIATION (fog): with nothing known to gather the citizen explores (the C++ classic's forest finder and
        -- centres read the whole map; the port sees what it has explored, a town centre's few tiles at the start)
        if not sent and u.action ~= "explore" then
            ai.explore({u.id})
            R.touch(u.id)
        end
    end
end

-- AssignGatherers: every 10 s
local function assign_gatherers(view, memory)
    local rm = R.state(memory)
    local ids = R.refresh_lists(memory)
    local records = {}
    if #ids > 0 then
        local cols = ai.get(ids, {"action", "target", "category", "x", "y", "gathers", "inside", "waiting", "id"})
        for k, id in ipairs(ids) do
            if cols.action[k] ~= nil then
                records[id] = {id = id, action = cols.action[k], target = cols.target[k], category = cols.category[k],
                               x = cols.x[k], y = cols.y[k], gathers = cols.gathers[k], inside = cols.inside[k],
                               waiting = cols.waiting[k]}
            end
        end
    end
    rebalance(view, memory, records)
    -- the listed gatherers that gather something else are moved to its list (the theMoved pass); idle ones collected
    local idle, boats = {}, {}
    for _, key in ipairs({"idle", "food", "wood", "stone", "gold", "iron"}) do
        local list = rm.lists[key]
        for k = #list, 1, -1 do
            local u = records[list[k]]
            local kind = u ~= nil and u.action == "gather" and u.gathers or nil
            if kind ~= nil and u.target ~= nil then
                local t = R.unit(u.target)
                if t ~= nil and t.kind ~= nil then
                    kind = t.kind   -- (the resource's own kind: a farm's or a bush's food)
                end
            end
            if kind ~= nil and kind ~= key then
                table.remove(list, k)
                R.add_gatherer(rm, u.id, kind)
            elseif u ~= nil and u.category == "boat" and key ~= "idle" and u.action ~= "gather" then
                -- a boat off its fish (the engine's naval flight: EECPRMNavalFlee gives the classic's own lists
                -- AddIdleGatherer, the script's lists are the port's): back to the idle list, where AssignIdleBoats
                -- sends it to fish again
                table.remove(list, k)
                R.add_gatherer(rm, u.id, "idle")
            end
        end
    end
    for _, key in ipairs({"idle", "food", "wood", "stone", "gold", "iron"}) do
        for _, id in ipairs(rm.lists[key]) do
            local u = records[id]
            if u ~= nil and u.inside == nil then
                if u.category == "boat" then
                    if key == "idle" then   -- (the classic: every boat of the idle list, whatever it does)
                        boats[#boats + 1] = u
                    end
                elseif F().busy(memory, u.id) then
                    -- (on a ferry trip: the landing has them)
                elseif u.action == "idle" or u.action == "explore" or (u.action == "gather" and u.waiting) then
                    -- (an exploring citizen: one with nothing known to gather; a gatherer waiting for room at its
                    -- resource, as the classic's idle list takes it)
                    idle[#idle + 1] = u
                end
            end
        end
    end
    local function take()
        return table.remove(idle, 1)
    end
    if #idle > 0 then
        for _, r in ipairs(RES) do
            local wanted = rm.wanted[r] - #rm.lists[r]
            -- (IsIslandExpansionDue: the minerals are looked at with none wanted, for the ferry below)
            local mineral = r == "stone" or r == "gold" or r == "iron"
            if wanted > 0 or (mineral and F().due(view, memory)) then
                if r == "food" and fishes(view, memory) and #boats < 8 and view.time >= rm.next_fish_boat then
                    request_boats(view, memory)
                    rm.next_fish_boat = view.time + 30000
                end
                if wanted > #idle + #boats then
                    wanted = #idle + #boats
                end
                if r == "food" then
                    local function bushes(needs_drop, near)
                        local target = R.find_farm(view, memory, needs_drop, near, idle[1])
                        while target ~= nil and wanted > 0 and #idle > 0 do
                            if R.gatherers(target.id) < T.b(memory, "mining_limit") then
                                local u = take()
                                R.send_to_gather(memory, u.id, target)
                                wanted = wanted - 1
                            else
                                target = R.find_farm(view, memory, needs_drop, near, idle[1])
                            end
                        end
                    end
                    bushes(true, true)
                    if wanted > 0 then
                        -- the idle farms
                        for _, f in ipairs(R.farms(memory, true, math.min(wanted, #idle))) do
                            local u = take()
                            if u == nil then
                                break
                            end
                            R.send_to_gather(memory, u.id, {id = f.id, kind = "food"})
                            wanted = wanted - 1
                        end
                    end
                    if wanted > 0 then
                        for _, a in ipairs(R.find_mines(view, memory, math.min(wanted, #idle), true)) do
                            local u = idle[1]
                            if u ~= nil and ai.reach(u.x, u.y, a.x, a.y) then
                                take()
                                R.send_to_gather(memory, u.id, a)
                                wanted = wanted - 1
                            end
                        end
                    end
                    if wanted > 0 then
                        bushes(false, true)
                    end
                    if wanted > 0 then
                        bushes(false, false)
                    end
                    if wanted > 0 then
                        for _, a in ipairs(R.find_mines(view, memory, math.min(wanted, #idle), false)) do
                            local u = idle[1]
                            if u ~= nil and ai.reach(u.x, u.y, a.x, a.y) then
                                take()
                                R.send_to_gather(memory, u.id, a)
                                wanted = wanted - 1
                            end
                        end
                    end
                    if wanted > 0 and not rm.uses_fishing then
                        -- the unfinished farms: the citizens build them
                        for _, f in ipairs(R.farms(memory, false, math.min(wanted, #idle))) do
                            local u = take()
                            if u == nil then
                                break
                            end
                            ai.repair({u.id}, f.id)
                            wanted = wanted - 1
                        end
                    end
                    if wanted > 0 and fishes(view, memory) then
                        for _, b in ipairs(boats) do
                            local dock = R.nearest_dock(b)
                            if dock == nil then
                                break
                            end
                            local fish = R.find_fish(memory, dock)
                            if fish == nil then
                                break
                            end
                            R.send_to_gather(memory, b.id, fish)
                            b.sent = true
                        end
                        local left = {}
                        for _, b in ipairs(boats) do
                            if not b.sent then
                                left[#left + 1] = b
                            end
                        end
                        boats = left
                    end
                elseif r == "wood" then
                    local target = R.find_forest_target(view, memory, wanted, idle[1])
                    if target ~= nil then
                        local n = math.min(wanted, #idle)
                        while n > 0 and #idle > 0 do
                            local u = take()
                            R.send_to_gather(memory, u.id, target)
                            n = n - 1
                        end
                    end
                else
                    local target = R.find_resource_target(view, memory, r, idle[1], true)
                    local n = math.min(wanted, #idle)
                    while target ~= nil and n > 0 and #idle > 0 do
                        if R.gatherers(target.id) < T.b(memory, "mining_limit") then
                            local u = take()
                            R.send_to_gather(memory, u.id, target)
                            n = n - 1
                        else
                            target = R.find_resource_target(view, memory, r, idle[1], true)
                        end
                    end
                    -- RequestFerry: wanted (or the island expansion due) on a water map, the resource's first target with
                    -- no unit's reach test; the idle citizens of the town's land that cannot walk to it are ferried
                    if (wanted > 0 or F().due(view, memory)) and F().water_map(memory) and #idle > 0 then
                        local far = R.find_resource_target(view, memory, r, nil, true)
                        local tx, ty = R.town(view)
                        if far ~= nil and tx ~= nil then
                            local stranded = {}
                            for _, u in ipairs(idle) do
                                if ai.reach(tx, ty, u.x, u.y) and not ai.reach(u.x, u.y, far.x, far.y) then
                                    stranded[#stranded + 1] = u
                                end
                            end
                            if #stranded > 0 and F().request(view, memory, stranded, far) then
                                local keep = {}
                                for _, u in ipairs(idle) do
                                    if not F().busy(memory, u.id) then
                                        keep[#keep + 1] = u
                                    end
                                end
                                idle = keep
                            end
                        end
                    end
                end
            end
        end
        if #idle > 0 then
            assign_idle_gatherers(view, memory, idle)
        end
    end
    -- AssignIdleBoats
    for _, b in ipairs(boats) do
        local dock = R.nearest_dock(b)
        if dock ~= nil then
            local fish = R.find_fish(memory, dock)
            if fish ~= nil then
                R.send_to_gather(memory, b.id, fish)
            elseif b.action == "idle" then
                -- New (AI_RESEARCH 8.89; DEVIATION, fog): no fish known, the boat explores the water (the scout)
                ai.explore({b.id})
                R.touch(b.id)
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- New (AI_RESEARCH 8.89): the Stone Age dock and its scout boat

-- From the Stone Age, on any map with water, a dock when the player has none (built or unfinished) and none is
-- queued, once a dock site is known; checked every 30 s. The classic's dock comes from the building counts (the
-- first military buildings, which the managers' rotation leaves off for long stretches) or from the coast's boats
-- (fish known near the town). DEVIATION (fog): the classic reads the map's fish from the start; the port knows only
-- what it has explored, and nothing explores the water before a dock and a boat exist.
-- With a dock built, no fishing boat and none requested or being made, while no fish is known to work (fishes),
-- one boat is requested: the scout. An idle boat with no fish to go to explores (AssignIdleBoats above), as an
-- idle citizen with nothing known to gather does (assign_idle_gatherers).
local DOCK_CHECK_MS = 30000

local function dock_check(view, memory)
    local rm = R.state(memory)
    if view.time < (rm.next_dock_check or 0) then
        return
    end
    rm.next_dock_check = view.time + DOCK_CHECK_MS
    if view.epoch < 1 or memory.t.waterless then
        return
    end
    local tech = U().class_tech(view, C.dock)
    if tech == nil or tech.obsolete or not tech.available or view.epoch < tech.epoch then
        return
    end
    if #ai.units{type = C.dock} == 0 then
        if not B().is_queued(memory, C.dock) and #B().water_sites(view, memory) > 0 then
            B().queue(view, memory, C.dock, false, true)
            print("classic_lua dock.stone queue")
        end
        return
    end
    if #ai.units{type = C.dock, built = true} == 0 or fishes(view, memory) then
        return
    end
    local boat = T.fishing_boat_class(view.epoch)
    if #ai.units{type = boat} == 0 and not U().is_requested(memory, boat) and not U().in_production(view, boat) then
        U().request(view, memory, boat, 1)
        print("classic_lua dock.stone scout")
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- New (AI_RESEARCH 8.97): the island food. On Large Islands the food fell from about 114 a minute (40-60 min) to 59
-- (75-90 min): the forage bushes of the small island run out at 60-80 minutes, the granary (whose foundations are the
-- farms) waited for the building counts' rotation (a median 78 minutes, 20 after the Copper Age), the boats were
-- requested only while idle citizens were being assigned and then waited behind the epoch's reserve (a town had a dock
-- and no boat all game; units.lua can_afford now pays the island's boats, granary and farms from the stock), a dock
-- stood with its first boat queued for 50 minutes, and the coast's boats counted only the fish within 30 tiles of the
-- town, so the boats worked the fish by the first dock out and then sailed 20-30 tiles to the next. On island and
-- planets maps, every 30 s from the Stone Age:
--  - a stuck dock replaced (island_stuck_docks below);
--  - the boats: with a dock built and fish to work (fishes), one more boat requested while the boats (every fishing
--    class), requested or being made, are fewer than the coast's boats wanted (EECPRMRequestBoats' count);
--  - a second dock: while the fish within FISH_NEAR tiles of the docks hold less than FISH_LOW, a dock at the
--    nearest placeable site of the town's land within 8 tiles of the explored fish with the most food left within
--    FISH_NEAR of it (FISH_RICH at least, more than FISH_NEAR tiles from every dock), DOCKS_MAX docks at most;
--  - the granary: from the epoch of the farms, with no granary (built or not) and none queued, when the forage
--    bushes left on the town's land hold less than BUSH_LOW food.
-- DEVIATION (fog): the fish and bushes are the explored ones.
local ISLAND_FOOD_MS = 30000
local FISH_NEAR, FISH_LOW, FISH_RICH, DOCKS_MAX, BUSH_LOW = 12, 1500, 1800, 3, 3000

local function fishing_boats()
    local n = 0
    for _, c in ipairs({"s02_fishing_boat_stone", "s04_fishing_boat_bronze", "s11_fishing_boat_modern"}) do
        n = n + #ai.units{type = c}
    end
    return n
end

local function island_boats(view, memory)
    if #ai.units{type = C.dock, built = true} == 0 or not fishes(view, memory) then
        return
    end
    local boat = T.fishing_boat_class(view.epoch)
    local um = U().state(memory)
    local wanted = M.coast_boats_wanted(view, memory)
    if (um.waiting[boat] or 0) == 0 and not U().in_production(view, boat) and fishing_boats() < wanted then
        U().request(view, memory, boat, 1)
    end
end

local function island_dock(view, memory)
    local docks = {}
    for _, id in ipairs(ai.units{type = C.dock}) do
        local u = R.unit(id)
        if u ~= nil then
            docks[#docks + 1] = u
        end
    end
    if #docks == 0 or #docks >= DOCKS_MAX or B().is_queued(memory, C.dock) then
        return
    end
    for _, d in ipairs(docks) do
        if not d.built then
            return
        end
    end
    local fish = {}
    for _, id in ipairs(R.state(memory).fish) do
        local u = R.unit(id)
        if u ~= nil and (u.amount or 0) > 0 then
            fish[#fish + 1] = u
        end
    end
    local near, r2 = 0, FISH_NEAR * FISH_NEAR
    for _, u in ipairs(fish) do
        for _, d in ipairs(docks) do
            if R.d2(u.x, u.y, d.x, d.y) <= r2 then
                near = near + u.amount
                break
            end
        end
    end
    if near >= FISH_LOW then
        return
    end
    -- the fish with the most food within FISH_NEAR of it, away from the docks (ties: the lower id)
    local best, bs
    for _, u in ipairs(fish) do
        local free = true
        for _, d in ipairs(docks) do
            if R.d2(u.x, u.y, d.x, d.y) <= r2 then
                free = false
                break
            end
        end
        if free then
            local sum = 0
            for _, v in ipairs(fish) do
                if R.d2(u.x, u.y, v.x, v.y) <= r2 then
                    sum = sum + v.amount
                end
            end
            if sum >= FISH_RICH and (bs == nil or sum > bs or (sum == bs and u.id < best.id)) then
                best, bs = u, sum
            end
        end
    end
    if best == nil then
        return
    end
    local tx, ty = R.town(view)
    for _, s in ipairs(ai.water_sites(best.x, best.y, {max = 16})) do
        local x, y = s[1], s[2]
        if R.d2(x, y, best.x, best.y) > 64 then
            break
        end
        local apart = true
        for _, d in ipairs(docks) do
            if R.d2(x, y, d.x, d.y) <= 75 then
                apart = false
            end
        end
        if apart and tx ~= nil and ai.reach(tx, ty, x, y) and ai.can_place(C.dock, x, y) and not R.enemy_near(x, y)
                and not B().stalled_site(view, memory, C.dock, x, y) then
            B().queue(view, memory, C.dock, true, false, x, y)
            print(string.format("classic_lua dock.fish queue at=%d,%d fish=%d near=%d", x, y, bs, near))
            return
        end
    end
end

local function island_granary(view, memory)
    if R.state(memory).uses_fishing or #ai.units{type = C.granary} > 0 or B().is_queued(memory, C.granary) then
        return
    end
    local farm = memory.civ ~= nil and memory.civ.farm_type or C.farm
    local tech = U().class_tech(view, farm)
    if tech == nil or tech.obsolete or not tech.available or view.epoch < tech.epoch then
        return
    end
    local tx, ty = R.town(view)
    if tx == nil then
        return
    end
    local left = 0
    for _, id in ipairs(ai.resources{kind = "food"}) do
        local u = R.unit(id)
        if u ~= nil and u.type == C.forage_bush and (u.amount or 0) > 0 and ai.reach(tx, ty, u.x, u.y) then
            left = left + u.amount
            if left >= BUSH_LOW then
                return
            end
        end
    end
    B().queue(view, memory, C.granary, false, true)
    print(string.format("classic_lua granary.island queue bushes=%d", left))
end

-- a dock that has been making a unit for DOCK_STUCK_MS while no unit of that category came out (a fishing boat takes
-- 30 s) cannot let its ships out (a Large Islands dock stood on a shore whose water the new ship could not reach, its
-- boat queued for 50 minutes): it is deleted and its site never used again (build.lua's stalls, forever);
-- dock_check and island_dock build the next
local DOCK_STUCK_MS = 300000

local function island_stuck_docks(view, memory)
    local rm = R.state(memory)
    rm.dock_prod = rm.dock_prod or {}
    local seen = {}
    local tree = U().tree(view)
    for _, id in ipairs(ai.units{type = C.dock, built = true}) do
        local u = R.unit(id)
        local t = u ~= nil and u.producing ~= nil and tree.byId[u.producing] or nil
        if t ~= nil and t.makes ~= nil then
            seen[id] = true
            -- (by the class's category: a unit made is upgraded at once to the epoch's class of it)
            local k = ai.class(t.makes)
            local made = #ai.units{category = k ~= nil and k.category or "ship"}
            local p = rm.dock_prod[id]
            -- (at the population cap the engine holds the unit back: no fault of the dock's)
            local full = view.population + 2 >= view.population_cap
            if p == nil or p.item ~= u.producing or made ~= p.made or full then
                rm.dock_prod[id] = {item = u.producing, made = made, t = view.time}
            elseif view.time - p.t >= DOCK_STUCK_MS then
                rm.dock_prod[id] = nil
                B().forbid_site(memory, C.dock, u.x, u.y)
                ai.delete({id})
                print(string.format("classic_lua dock.stuck at=%d,%d item=%s", u.x, u.y, t.makes))
            end
        end
    end
    for _, id in ipairs(ai.keys(rm.dock_prod)) do
        if not seen[id] then
            rm.dock_prod[id] = nil
        end
    end
end

local function island_food(view, memory)
    local rm = R.state(memory)
    if view.time < (rm.next_island_food or 0) then
        return
    end
    rm.next_island_food = view.time + ISLAND_FOOD_MS
    if view.epoch < 1 or not F().water_map(memory) then
        return
    end
    island_stuck_docks(view, memory)
    island_boats(view, memory)
    island_dock(view, memory)
    island_granary(view, memory)
end

-- ---------------------------------------------------------------------------------------------------------------
-- Update

function M.update(view, memory)
    local rm = R.state(memory)
    if not rm.init then
        R.refresh_resources(view, memory)
        rm.next_fish_boat = view.time + 30000
        rm.next_wanted = view.time + 5000
        rm.next_assign = view.time + 10000
        rm.next_check = view.time + 15000
        rm.init = true
    end
    R.spend_income(view, memory)
    dock_check(view, memory)
    island_food(view, memory)
    F().step(view, memory)
    settlement_for(view, memory)
    if view.time >= rm.next_wanted then
        update_wanted(view, memory)
        rm.next_wanted = view.time + 5000
    end
    if view.time >= rm.next_assign then
        assign_gatherers(view, memory)
        rm.next_assign = view.time + 10000
    end
    if view.time >= rm.next_check then
        -- (the port's fog: the centres and the game grow with what is explored)
        R.update_groups(view, memory)
        R.refresh_resources(view, memory)
        rm.next_check = view.time + 15000
    end
    -- the danger zones expire after 30 s
    if rm.danger ~= nil then
        for k = #rm.danger, 1, -1 do
            if view.time - rm.danger[k].t > 30000 then
                table.remove(rm.danger, k)
            end
        end
    end
end

return M

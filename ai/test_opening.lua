-- TEST ONLY: the next engine's test opening, the Lua port of the C++ one (src/Empire Earth/AI Thread/EECPNextTest.cpp;
-- docs/AI_LUA_PLAN.md §5 S2 gate: both must play the same game). A fixed boom opening from the Prehistoric toward the
-- Copper Age that uses nothing but the view and the orders. It proves the tactics layer and the Lua contract; it is
-- not a good AI (stage S3 writes boom.lua). Keep it in step with the C++ opening.
local V = require("lib.view")

local CITIZENS = {25, 40, 50}   -- the citizens wanted in the Prehistoric, Stone and later
local SOLDIERS = {4, 8, 8}      -- the mobile defence
local TOWERS = {2, 4, 4}
local BOATS = 6
local CLAIM_MS = 10000          -- a claim and gather pass every 10 s
local ATTACK_MS = 10 * 60000    -- the one attack: this long into the Stone Age

-- the gatherer shares in percent per epoch (food takes the rest): Prehistoric, Stone, later
local SHARES = {
    {food = 0, wood = 20, stone = 0, gold = 10, iron = 15},
    {food = 0, wood = 22, stone = 8, gold = 12, iron = 15},
    {food = 0, wood = 20, stone = 10, gold = 12, iron = 15},
}

local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle"}
-- not built for the SOL (as the classic ResearchCheapest)
local NOT_FOR_SOL = {b_town_center = true, b_capitol = true, b_settlement = true, b_farm = true, b_granary = true}

-- the cheapest military unit the barracks make (the Prehistoric's clubman)
local function soldier_class(view)
    local best, best_cost = nil, 0
    for _, c in ipairs(view.costs) do
        if not c.building and c.military and c.producer == "b_barracks" then
            local s = V.sum(c.cost)
            if best == nil or s < best_cost then
                best, best_cost = c.type, s
            end
        end
    end
    return best
end

local function tower_class(view)
    for t = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[t]) ~= nil then
            return TOWER_CLASSES[t]
        end
    end
    return nil
end

-- the nearest unclaimed site of a resource in the claim range with no danger (nil: none)
local function nearest_free(view, resource, patch, range)
    local best = nil
    for _, s in ipairs(view.sites) do
        if not (s.claimed or s.resource ~= resource or s.units == 0 or s.danger > 0 or (patch and not s.patch)
                or s.distance > range) then
            if best == nil or s.distance < best.distance then
                best = s
            end
        end
    end
    return best
end

local function claimed(view, resource, patch)
    local n = 0
    for _, s in ipairs(view.sites) do
        if s.claimed and s.resource == resource and s.units > 0 and (not patch or s.patch) then
            n = n + 1
        end
    end
    return n
end

local M = {}

function M.step(view, memory)
    local orders = {}
    local function order(o)
        orders[#orders + 1] = o
    end
    memory.step = (memory.step or 0) + 1
    if view.town_x < 0 then
        return orders   -- no town centre: nothing to build on
    end
    local age = (view.epoch < 2 and view.epoch or 2) + 1
    local citizen = V.cost(view, "citizen")

    -- saving for the next epoch once the town has its citizens: the reserve holds the advance's cost, and the advance
    -- is asked for only when it is paid
    local last = view.epoch >= view.last_epoch
    local saving = not last and not view.advancing and view.citizens >= CITIZENS[age] - 2
    local reserve, differs = {}, false
    for _, r in ipairs(V.resources) do
        reserve[r] = saving and view.next_epoch_cost[r] or 0
        differs = differs or reserve[r] ~= view.reserve[r]
    end
    if differs then
        order({verb = "reserve", food = reserve.food, wood = reserve.wood, stone = reserve.stone, gold = reserve.gold,
               iron = reserve.iron})
    end
    if saving and view.sol_missing == 0 then
        local paid = true
        for _, r in ipairs(V.resources) do
            paid = paid and view.stock[r] >= view.next_epoch_cost[r]
        end
        if paid then
            order({verb = "advance"})
        end
    end

    -- citizens: two at a time from the town centres up to the epoch's count
    local making = citizen and citizen.in_production or 0
    local waiting = citizen and citizen.requested or 0
    local planned = view.citizens + making + waiting
    if citizen and waiting == 0 and making < 2 * citizen.producers and planned < CITIZENS[age]
            and view.population < view.population_cap then
        order({verb = "train", type = "citizen", n = math.min(CITIZENS[age] - planned, 2)})
    end

    local last_claim = memory.last_claim or 0
    if view.time - last_claim >= CLAIM_MS or last_claim == 0 then
        memory.last_claim = view.time
        -- the gatherers in fixed shares; while saving, the shares follow what the advance still lacks
        local land = view.citizens
        local want, lacking, lack_sum = {}, {}, 0
        for _, r in ipairs(V.resources) do
            lacking[r] = math.max(view.next_epoch_cost[r] - view.stock[r], 0)
            lack_sum = lack_sum + lacking[r]
        end
        local others = 0
        for i = 2, #V.resources do
            local r = V.resources[i]
            if saving and lack_sum > 0 then
                want[r] = lacking[r] > 0 and (land * lacking[r] + lack_sum - 1) // lack_sum or 0
            else
                want[r] = (land * SHARES[age][r] + 50) // 100
            end
            others = others + want[r]
        end
        want.food = land > others and land - others or 0
        for _, r in ipairs(V.resources) do
            if want[r] ~= view.wanted[r] then
                order({verb = "gather", resource = r, n = want[r]})
            end
        end

        -- claims: forage patches with room for the food gatherers (two at least, four at most), and a site of each
        -- mineral the gatherers want; the claim grows when no patch is left in it
        local range = view.claim_range
        local patches = claimed(view, "food", true)
        local patch_room = 0
        for _, s in ipairs(view.sites) do
            if s.claimed and s.patch and s.units > 0 then
                patch_room = patch_room + s.room
            end
        end
        if patches < 2 or (patches < 4 and patch_room < want.food - view.gatherers.food + 2) then
            local site = nearest_free(view, "food", true, range)
            if site then
                order({verb = "claim", site = site.index})
            elseif patches < 2 and range < view.claim_range_max then
                order({verb = "claim_range", range = range + view.claim_range_step})
            end
        end
        for i = 3, #V.resources do
            local r = V.resources[i]
            if want[r] ~= 0 and claimed(view, r, false) == 0 then
                local site = nearest_free(view, r, false, range)
                if site then
                    order({verb = "claim", site = site.index})
                end
            end
        end

        -- a settlement at a worked forage patch with no drop site at hand (one at a time, two at most)
        local settlements = V.buildings(view, "b_settlement")
        local settling = settlements ~= nil and settlements.unfinished + settlements.queued > 0
        if not settling and V.total(settlements) < 2 and V.cost(view, "b_settlement") ~= nil then
            for _, s in ipairs(view.sites) do
                if s.claimed and s.patch and not s.has_drop and s.danger == 0 then
                    order({verb = "build", type = "b_settlement", near = "site", site = s.index})
                    break
                end
            end
        end

        -- towers at the worked sites, one at a time
        local tower = tower_class(view)
        local towers = tower and V.buildings(view, tower) or nil
        local tower_busy = towers ~= nil and towers.unfinished + towers.queued > 0
        if tower and not tower_busy and view.citizens >= 15 and V.total(towers) < TOWERS[age] then
            for _, s in ipairs(view.sites) do
                if s.claimed and s.workers > 0 and not s.has_tower then
                    order({verb = "build", type = tower, near = "site", site = s.index})
                    break
                end
            end
        end

        -- the next epoch's SOL: one building at a time that counts toward it and is not built yet (as the classic
        -- ResearchCheapest: no town centres, settlements or farms), the cheapest first
        if saving and view.sol_missing > 0 then
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

        -- from the Stone Age: a dock and fishing boats
        if view.epoch >= 1 and not saving then
            order({verb = "fish", boats = BOATS, docks = 1})
        end
    end

    -- a barracks, then the mobile defence
    local barracks = V.buildings(view, "b_barracks")
    if view.citizens >= 12 and V.total(barracks) == 0 then
        order({verb = "build", type = "b_barracks", near = "town"})
    end
    local soldier = soldier_class(view)
    if barracks ~= nil and barracks.finished > 0 and soldier ~= nil and not saving then
        local c = V.cost(view, soldier)
        local have = view.army_count + (c and c.requested + c.in_production or 0)
        if have < SOLDIERS[age] then
            order({verb = "train", type = soldier, n = SOLDIERS[age] - have})
        end
    end
    -- the four soldiers guard the town; ten minutes into the Stone Age four of them attack the first enemy seen
    if not memory.defended and view.army_count >= 4 then
        memory.defended = true
        order({verb = "defend", n = 4})
    end
    if not memory.attacked and view.epoch >= 1 and view.army_count >= 6 and view.time - view.epoch_start >= ATTACK_MS then
        local target = view.enemies[1] and view.enemies[1].player or -1
        if target > 0 then
            memory.attacked = true
            order({verb = "attack", player = target, n = 4})
        end
    end
    return orders
end

return M

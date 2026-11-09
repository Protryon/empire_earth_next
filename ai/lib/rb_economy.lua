-- The economy of the rush and balanced scripts (docs/AI_LUA_PLAN.md §5 "S4 as built"): citizens, the gatherer split,
-- claims, drop sites, towers, docks and boats, the buildings a plan lists, the epoch's SOL and the advance. The numbers
-- are the caller's plan (one entry per age: 1 Prehistoric, 2 Stone, 3 Copper and later). require("lib.rb_economy").
--
-- A plan:
--   citizens[age]                 the citizens wanted
--   shares[age] = {wood=, stone=, gold=, iron=}     percent of the citizens (food takes the rest)
--   least[age], most[age]         per resource: gatherers at least / at most (food: at least food_least share)
--   towers[age], tower_citizens   towers at the worked sites, from this many citizens
--   settlements                   the most drop sites at forage patches
--   boats[age], docks[age]        fishing (from the Stone Age)
--   buildings[age] = {{type=, n=, citizens=}, ...}  built up to n once the town has that many citizens
--   save_slack                    saving for the next epoch starts this many citizens short of the target
--   save_after[age]               ... or this many ms into the epoch, whichever comes first (nil: never by time)
local V = require("lib.view")
local M = {}

local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle"}
-- not built for the SOL (as the classic ResearchCheapest)
local NOT_FOR_SOL = {b_town_center = true, b_capitol = true, b_settlement = true, b_farm = true, b_granary = true,
                     b_dock = true}
local CLAIM_MS = 10000

function M.age(view)
    return (view.epoch < 2 and view.epoch or 2) + 1
end

function M.tower_class(view)
    for t = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[t]) ~= nil then
            return TOWER_CLASSES[t]
        end
    end
    return nil
end

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

local function clamp(v, lo, hi)
    if hi ~= nil and v > hi then
        v = hi
    end
    if lo ~= nil and v < lo then
        v = lo
    end
    return v
end

-- whether the town saves for the next epoch now (the plan's rule; opts.saving overrides it)
function M.saving(view, plan, opts)
    if view.epoch >= view.last_epoch or view.advancing then
        return false
    end
    if opts ~= nil and opts.saving ~= nil then
        return opts.saving
    end
    local age = M.age(view)
    if view.citizens >= plan.citizens[age] - (plan.save_slack or 2) then
        return true
    end
    local after = plan.save_after and plan.save_after[age]
    return after ~= nil and view.time - view.epoch_start >= after
end

-- the economy's orders for this step (appended to orders); returns whether it saves for the next epoch, and whether
-- the town is short of its citizens
function M.step(view, memory, plan, orders, opts)
    local function order(o)
        orders[#orders + 1] = o
    end
    memory.eco = memory.eco or {}
    local mem = memory.eco
    local age = M.age(view)
    local citizen = V.cost(view, "citizen")
    local saving = M.saving(view, plan, opts)

    -- the reserve holds the advance's cost while saving; the advance is asked for only when it is paid
    local reserve, differs = {}, false
    for _, r in ipairs(V.resources) do
        reserve[r] = saving and view.next_epoch_cost[r] or 0
        if opts ~= nil and opts.reserve ~= nil then
            reserve[r] = reserve[r] + (opts.reserve[r] or 0)
        end
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

    -- citizens: two at a time from each town centre up to the epoch's count (never held back for the army)
    local want_citizens = plan.citizens[age]
    local making = citizen and citizen.in_production or 0
    local waiting = citizen and citizen.requested or 0
    local planned = view.citizens + making + waiting
    if citizen and waiting == 0 and making < 2 * citizen.producers and planned < want_citizens
            and view.population < view.population_cap and not view.advancing then
        order({verb = "train", type = "citizen", n = math.min(want_citizens - planned, 2)})
    end

    if view.time - (mem.last_claim or 0) >= CLAIM_MS or (mem.last_claim or 0) == 0 then
        mem.last_claim = view.time
        -- the gatherers: the plan's shares within its bounds; while saving, the minerals follow what the advance lacks
        local land = view.citizens
        local want, lacking, lack_sum = {}, {}, 0
        for i = 2, #V.resources do
            local r = V.resources[i]
            lacking[r] = math.max(view.next_epoch_cost[r] - view.stock[r], 0)
            lack_sum = lack_sum + lacking[r]
        end
        local least = plan.least[age] or {}
        local most = plan.most[age] or {}
        local others = 0
        for i = 2, #V.resources do
            local r = V.resources[i]
            local w = (land * (plan.shares[age][r] or 0) + 50) // 100
            if saving then
                local need = (lack_sum > 0 and lacking[r] > 0) and (land * lacking[r] + lack_sum - 1) // (lack_sum * 2) or 0
                w = need   -- (the plan's least below: what the advance does not need goes to food)
            end
            want[r] = clamp(w, least[r], most[r])
            others = others + want[r]
        end
        local food_least = (land * (plan.food_least or 40) + 50) // 100
        if land - others < food_least then
            -- food first: the others give way, the largest first
            local over = food_least - (land - others)
            for _ = 1, 50 do
                if over <= 0 then
                    break
                end
                local big = nil
                for i = 2, #V.resources do
                    local r = V.resources[i]
                    if want[r] > (least[r] or 0) and (big == nil or want[r] > want[big]) then
                        big = r
                    end
                end
                if big == nil then
                    break
                end
                want[big] = want[big] - 1
                others = others - 1
                over = over - 1
            end
        end
        want.food = land > others and land - others or 0
        -- the forage patches: what the claim holds, and whether one is still free in it
        local range = view.claim_range
        local patches = claimed(view, "food", true)
        local patch_room = 0
        for _, s in ipairs(view.sites) do
            if s.claimed and s.patch and s.units > 0 then
                patch_room = patch_room + s.room
            end
        end
        local free_patch = nearest_free(view, "food", true, range)
        -- no patch left to claim: the food gatherers hunt (the game near home first), and the claim grows
        local prefer = (patches == 0 and free_patch == nil) and "hunt" or "patch"
        for _, r in ipairs(V.resources) do
            if want[r] ~= view.wanted[r] or (r == "food" and prefer ~= (mem.prefer or "patch")) then
                if r == "food" then
                    order({verb = "gather", resource = r, n = want[r], prefer = prefer})
                    mem.prefer = prefer
                else
                    order({verb = "gather", resource = r, n = want[r]})
                end
            end
        end

        -- claims: forage patches with room for the food gatherers (two at least, four at most), and a site of each
        -- mineral the gatherers want; the claim grows when no patch is left in it
        if patches < 2 or (patches < 4 and patch_room < want.food - view.gatherers.food + 2) then
            if free_patch then
                order({verb = "claim", site = free_patch.index})
            elseif range < view.claim_range_max then
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

        -- a drop site at a worked forage patch without one (one at a time)
        local settlements = V.buildings(view, "b_settlement")
        local settling = settlements ~= nil and settlements.unfinished + settlements.queued > 0
        if not settling and V.total(settlements) < (plan.settlements or 2) and V.cost(view, "b_settlement") ~= nil then
            for _, s in ipairs(view.sites) do
                if s.claimed and s.patch and not s.has_drop and s.danger == 0 then
                    order({verb = "build", type = "b_settlement", near = "site", site = s.index})
                    break
                end
            end
        end

        -- towers at the worked sites, one at a time
        local tower = M.tower_class(view)
        local towers = tower and V.buildings(view, tower) or nil
        local tower_busy = towers ~= nil and towers.unfinished + towers.queued > 0
        local tower_want = plan.towers[age] or 0
        if opts ~= nil and opts.towers ~= nil then
            tower_want = math.max(tower_want, opts.towers)
        end
        if tower and not tower_busy and view.citizens >= (plan.tower_citizens or 15) and V.total(towers) < tower_want then
            local placed = false
            for _, s in ipairs(view.sites) do
                if s.claimed and s.workers > 0 and not s.has_tower then
                    order({verb = "build", type = tower, near = "site", site = s.index})
                    placed = true
                    break
                end
            end
            if not placed then
                order({verb = "build", type = tower, near = "town"})
            end
        end

        -- the next epoch's SOL: one building at a time that counts toward it and is not built yet, the cheapest first
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

        -- fishing from the Stone Age
        local boats = plan.boats and plan.boats[age] or 0
        if view.epoch >= 1 and boats > 0 then
            order({verb = "fish", boats = boats, docks = plan.docks and plan.docks[age] or 1})
        end

        -- the plan's buildings, one of each type at a time
        for _, b in ipairs(plan.buildings[age] or {}) do
            if view.citizens >= (b.citizens or 0) and V.cost(view, b.type) ~= nil then
                local have = V.buildings(view, b.type)
                local busy = have ~= nil and have.unfinished + have.queued > 0
                if not busy and V.total(have) < b.n then
                    order({verb = "build", type = b.type, near = "town"})
                end
            end
        end
    end
    return saving, view.citizens < want_citizens - 2
end

return M

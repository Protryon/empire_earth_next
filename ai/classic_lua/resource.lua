-- EECPResourceManager and EECPResourceCenter in Lua (the classic port; ORIGIN in this folder). The gatherers per
-- resource, the needs ledger and the split of the gatherers over the resources (the original's needs split, or the
-- personality's demand split: economy.demand_split), the finders (the resource centres' mines and bushes, the forest
-- nearest the town, the game, farms, fish), the assignment every 10 s, the settlements at worked centres.
-- require("classic_lua.resource").
--
-- Memory (memory.rm): needs / wanted (by resource name), lists (the gatherers by resource and "idle", ids in order),
-- centres (the resource centres: {u = resource ids, active, x, y}), grouped (resource id -> centre index), mines (the
-- huntable animals), fish, the timers.
local T = require("classic_lua.tune")
local C = T.C

local M = {}

M.RES = {"food", "wood", "stone", "gold", "iron"}
local RES = M.RES
local MINERALS = {stone = true, gold = true, iron = true}

-- ---------------------------------------------------------------------------------------------------------------
-- small helpers

local function d2(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx * dx + dy * dy
end
M.d2 = d2

local function remove_value(list, v)
    for k = #list, 1, -1 do
        if list[k] == v then
            table.remove(list, k)
        end
    end
end

-- the per-step caches (not saved): the unit records and the own units by filter, cleared every step (begin_step) and
-- for a unit when it is ordered (touch); the script's charges stay well under the step's budget with them
local sStep = {time = -1, units = {}, lists = {}, memo = {}}

function M.begin_step(view)
    sStep = {time = view.time, units = {}, lists = {}, memo = {}}
end

-- ai.unit through the step's cache (false: known gone)
function M.unit(id)
    local u = sStep.units[id]
    if u == nil then
        u = ai.unit(id) or false
        sStep.units[id] = u
    end
    return u or nil
end

function M.touch(id)
    sStep.units[id] = nil
end

-- ai.units{type = t} through the step's cache (the own units of a class)
function M.of_type(t)
    local l = sStep.lists[t]
    if l == nil then
        l = ai.units{type = t}
        sStep.lists[t] = l
    end
    return l
end

-- a value computed once a step (key -> fn())
function M.memo(key, fn)
    local v = sStep.memo[key]
    if v == nil then
        v = fn()
        sStep.memo[key] = v
    end
    return v
end

-- the own gatherers sent to a resource (ai.gatherers, once a step; raised as the step sends more)
function M.gatherers(id)
    local map = M.memo("gatherers", ai.gatherers)
    return map[id] or 0
end

function M.add_gatherers(id, n)
    local map = M.memo("gatherers", ai.gatherers)
    map[id] = (map[id] or 0) + n
end

-- the class record cache of this game (the family, the attack): not saved, rebuilt on demand
local sClass = {}
local function class_of(name)
    local c = sClass[name]
    if c == nil then
        c = ai.class(name) or {}
        sClass[name] = c
    end
    return c
end
M.class_of = class_of

local FAMILY_ANIMAL, FAMILY_FISH = 0x11, 0x12

function M.state(memory)
    local rm = memory.rm
    if rm == nil then
        rm = {needs = {}, wanted = {}, lists = {idle = {}}, centres = {}, grouped = {}, mines = {}, fish = {},
              last_income = {}, init = false, next_wanted = 0, next_assign = 0, next_check = 0, next_fish_boat = 0,
              settle_enabled = false, claim_steps = 0, claim_last = 0, next_group = 0}
        for _, r in ipairs(RES) do
            rm.needs[r] = 0
            rm.wanted[r] = 0
            rm.lists[r] = {}
        end
        memory.rm = rm
    end
    return rm
end

-- the town centre tile (the build manager's mTownCenterTile): the view's
local function town(view)
    if view.town_x == nil or view.town_x < 0 then
        return nil
    end
    return view.town_x, view.town_y
end
M.town = town

-- ---------------------------------------------------------------------------------------------------------------
-- the ledger (EECPResourceManager::AddCosts / RemoveCosts / Earn / Spend)

-- AddCosts(node, n): a cost table (by resource name, scaled as the player pays) times n into the needs
function M.add_costs(memory, cost, n)
    local rm = M.state(memory)
    for _, r in ipairs(RES) do
        local c = cost[r] or 0
        if c > 0 then
            rm.needs[r] = rm.needs[r] + c * n
        end
    end
end

function M.remove_costs(memory, cost, n)
    local rm = M.state(memory)
    for _, r in ipairs(RES) do
        local c = cost[r] or 0
        if c > 0 then
            rm.needs[r] = rm.needs[r] - c * n
        end
    end
end

-- Spend: what was gathered since the last step lowers the needs (EEComputerPlayer::AddResource -> Spend; the view's
-- income_total is the same sum)
function M.spend_income(view, memory)
    local rm = M.state(memory)
    local total = view.income_total
    if total == nil then
        return
    end
    for _, r in ipairs(RES) do
        local last = rm.last_income[r]
        if last ~= nil and total[r] > last then
            rm.needs[r] = rm.needs[r] - (total[r] - last)
        end
        rm.last_income[r] = total[r]
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the gatherer lists (AddGatherer / RemoveGatherer / AddIdleGatherer / CitizenLost)

function M.remove_gatherer(rm, id)
    for _, list in pairs(rm.lists) do
        remove_value(list, id)
    end
end

function M.add_gatherer(rm, id, r)
    local list = rm.lists[r]
    for _, v in ipairs(list) do
        if v == id then
            return
        end
    end
    list[#list + 1] = id
end

function M.add_idle(rm, id)
    M.remove_gatherer(rm, id)
    M.add_gatherer(rm, id, "idle")
end

-- the gatherers listed: drops the dead; new citizens and boats on the idle list (UnitAdded / AddAllUnits)
function M.refresh_lists(memory)
    local rm = M.state(memory)
    local alive = {}
    local ids = ai.units{category = "citizen"}
    for _, id in ipairs(ai.units{category = "boat"}) do
        ids[#ids + 1] = id
    end
    for _, id in ipairs(ids) do
        alive[id] = true
    end
    local known = {}
    for key, list in pairs(rm.lists) do
        for k = #list, 1, -1 do
            if not alive[list[k]] then
                table.remove(list, k)
            else
                known[list[k]] = true
            end
        end
    end
    for _, id in ipairs(ids) do
        if not known[id] then
            M.add_gatherer(rm, id, "idle")
        end
    end
    return ids
end

function M.count(rm, r)
    return #rm.lists[r]
end

function M.total_gatherers(rm)
    local n = #rm.lists.idle
    for _, r in ipairs(RES) do
        n = n + #rm.lists[r]
    end
    return n
end

-- ---------------------------------------------------------------------------------------------------------------
-- the resource centres (EEComputerPlayer::Regroup / UpdateGroups / FlagGroup, EECPResourceCenter)

-- a centre's middle: the middle of its units' bounding box (GetCenter without its free-tile search: DEVIATION, the
-- middle is only measured from)
local function centre_middle(c)
    local x1, y1, x2, y2
    for _, id in ipairs(c.u) do
        local u = M.unit(id)
        if u ~= nil then
            if x1 == nil then
                x1, y1, x2, y2 = u.x, u.y, u.x, u.y
            else
                x1, y1 = math.min(x1, u.x), math.min(y1, u.y)
                x2, y2 = math.max(x2, u.x), math.max(y2, u.y)
            end
        end
    end
    if x1 == nil then
        return c.x, c.y
    end
    return (x1 + x2) // 2, (y1 + y2) // 2
end
M.centre_middle = centre_middle

-- GetObjectsToGroup: the explored stone, gold and iron mines and forage bushes with something left, on the town's
-- land (any land on an island map); the count of fish. DEVIATION (fog): the original groups every resource of the
-- map at the start (it reads nature's unit list); the port groups what it has explored, again every 15 s
local function objects_to_group(view, memory)
    local tx, ty = town(view)
    local out, fish = {}, 0
    local island = memory.t.island or memory.t.planets
    for _, kind in ipairs({"food", "stone", "gold", "iron"}) do
        for _, id in ipairs(ai.resources{kind = kind}) do
            local u = M.unit(id)
            if u ~= nil then
                if kind == "food" then
                    if u.type == C.forage_bush then
                        if island or tx == nil or ai.reach(tx, ty, u.x, u.y) then
                            out[#out + 1] = u
                        end
                    elseif u.type == C.fish then
                        fish = fish + 1
                    end
                elseif island or tx == nil or ai.reach(tx, ty, u.x, u.y) then
                    out[#out + 1] = u
                end
            end
        end
    end
    return out, fish
end

-- UpdateGroups (Regroup the first time): the ungrouped units into the centre within the drop-off distance, else new
-- centres (inactive); the centres sorted by distance from the town centre; Regroup activates the centres within 6
-- drop-off distances of the town; later the inactive ones before the first active and between two active go active
function M.update_groups(view, memory)
    local rm = M.state(memory)
    local tx, ty = town(view)
    if tx == nil then
        return
    end
    local r = T.b(memory, "drop_off_distance")
    local r2 = r * r
    local first = #rm.centres == 0
    local units, fish = objects_to_group(view, memory)
    local made = false
    for _, u in ipairs(units) do
        if rm.grouped[u.id] == nil then
            local found
            for k, c in ipairs(rm.centres) do
                if d2(c.x, c.y, u.x, u.y) < r2 then
                    found = k
                    break
                end
            end
            if found ~= nil then
                local c = rm.centres[found]
                c.u[#c.u + 1] = u.id
                c.x, c.y = centre_middle(c)
            else
                rm.centres[#rm.centres + 1] = {u = {u.id}, active = false, x = u.x, y = u.y}
                made = true
            end
            rm.grouped[u.id] = true
        end
    end
    -- sorted by distance from the town centre (EECPCenterDistanceLess), stable
    table.sort(rm.centres, function(a, b)
        local da, db = d2(a.x, a.y, tx, ty), d2(b.x, b.y, tx, ty)
        if da ~= db then
            return da < db
        end
        return (a.u[1] or 0) < (b.u[1] or 0)
    end)
    if first then
        -- Regroup: the nearest centre adopts the town centre when one of its units is within the drop-off distance of
        -- it; else a centre of the town centre alone goes first (its units: what is grouped round it later)
        local adopted = false
        local c1 = rm.centres[1]
        if c1 ~= nil then
            for _, id in ipairs(c1.u) do
                local u = M.unit(id)
                if u ~= nil and d2(u.x, u.y, tx, ty) <= r2 then
                    adopted = true
                end
            end
        end
        if not adopted then
            table.insert(rm.centres, 1, {u = {}, active = true, x = tx, y = ty, town = true})
        end
        -- Regroup: active within 6 drop-off distances (and on the town's land on a water map)
        local island = memory.t.island or memory.t.planets
        for _, c in ipairs(rm.centres) do
            c.active = d2(c.x, c.y, tx, ty) < r2 * 36 and (not island or ai.reach(tx, ty, c.x, c.y))
        end
        rm.uses_fishing = fish >= 10 and memory.t.island
    elseif made then
        local firstActive
        for k, c in ipairs(rm.centres) do
            if c.active then
                firstActive = k
                break
            end
        end
        if firstActive ~= nil then
            for k = 1, firstActive - 1 do
                rm.centres[k].active = true
            end
        end
        for k = 2, #rm.centres - 1 do
            if not rm.centres[k].active and rm.centres[k - 1].active and rm.centres[k + 1].active then
                rm.centres[k].active = true
            end
        end
    end
    -- the grouped units that are gone leave their centre (a mine worked out)
    for _, c in ipairs(rm.centres) do
        for k = #c.u, 1, -1 do
            if M.unit(c.u[k]) == nil then
                rm.grouped[c.u[k]] = nil
                table.remove(c.u, k)
            end
        end
    end
end

-- FlagGroup: activates the first inactive centre (on a water map one whose land holds no known enemy capitol)
function M.flag_group(view, memory)
    local rm = M.state(memory)
    local island = memory.t.island or memory.t.planets
    for _, c in ipairs(rm.centres) do
        if not c.active and #c.u > 0 then
            local safe = true
            if island then
                for _, e in ipairs(ai.memory{enemy = true, type = C.capitol}) do
                    if ai.reach(e.x, e.y, c.x, c.y) then
                        safe = false
                    end
                end
            end
            if safe then
                c.active = true
                return
            end
        end
    end
end

-- FindResourceUnit: the centre's first unit of the resource with something left
local function find_resource_unit(c, r)
    for _, id in ipairs(c.u) do
        local u = M.unit(id)
        if u ~= nil and u.kind == r and (u.amount or 0) > 0 then
            return u
        end
    end
    return nil
end
M.find_resource_unit = find_resource_unit

-- the own drop sites (town centres, capitols, settlements; with food a granary): {id, x, y (the middle), built}
function M.drops(food)
    local out = {}
    local types = {C.town_center, C.capitol, C.settlement}
    if food then
        types[#types + 1] = C.granary
    end
    for _, t in ipairs(types) do
        local size = class_of(t).size or 0
        for _, id in ipairs(ai.units{type = t}) do
            local u = M.unit(id)
            if u ~= nil then
                out[#out + 1] = {id = id, x = u.x + size // 2, y = u.y + size // 2, built = u.built, size = size}
            end
        end
    end
    return out
end

-- EECPRMDropNear: a drop site within inRange (+ its half size) of a tile
function M.drop_near(x, y, range, food)
    for _, d in ipairs(M.drops(food)) do
        local reach = range + d.size // 2
        if d2(d.x, d.y, x, y) <= reach * reach then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- the claim range (economy.forage_patches with the claim range: EECPClaimRange / EECPClaimExpand)

local CLAIM, CLAIM_STEP, CLAIM_MAX = 25, 5, 45

function M.claim_range(memory)
    local rm = M.state(memory)
    local r = CLAIM + rm.claim_steps * CLAIM_STEP
    return r < CLAIM_MAX and r or CLAIM_MAX
end

function M.claim_expand(view, memory)
    local rm = M.state(memory)
    if rm.claim_last ~= 0 and view.time - rm.claim_last < 120000 then
        return
    end
    if M.claim_range(memory) >= CLAIM_MAX then
        return
    end
    rm.claim_steps = rm.claim_steps + 1
    rm.claim_last = view.time
end

function M.in_claim(view, memory, x, y)
    local tx, ty = town(view)
    if tx == nil then
        return true
    end
    local r = M.claim_range(memory)
    return d2(x, y, tx, ty) <= r * r
end

-- ---------------------------------------------------------------------------------------------------------------
-- the danger checks

-- EECPBuildManager::HasEnemyNear: armed enemy units in the box of a region (9 tiles) round a tile. With the
-- manifest's "sees_all" (ai/ais.json: the classic's information cheat) ai.enemy_near is the original, seen or not;
-- without it, DEVIATION (fog): the port counts the armed enemies it sees or remembers
M.sees_all = false
function M.enemy_near(x, y)
    if M.sees_all then
        return ai.enemy_near(x, y)
    end
    -- HasEnemyNear: any enemy influence (an armed unit, a tower, an armed building) in the 9-tile influence regions
    -- the box x +- 9, y +- 9 touches (up to 27 tiles across). DEVIATION (fog): the classic sums the influence table
    -- over every player's units; the port takes the armed enemies it sees or remembers (ai.memory: buildings until
    -- seen gone, units for 3 minutes)
    local list = M.memo("enemy_armed", function()
        local out = {}
        local recs = ai.memory{enemy = true, armed = true}
        for _, m in ipairs(recs) do
            out[#out + 1] = {x = m.x, y = m.y}
        end
        return out
    end)
    local l, r = (x - 9) // 9 * 9, (x + 9) // 9 * 9 + 8
    local t, b = (y - 9) // 9 * 9, (y + 9) // 9 * 9 + 8
    for _, m in ipairs(list) do
        if m.x >= l and m.x <= r and m.y >= t and m.y <= b then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- the finders

-- the room of a resource unit: fewer gatherers sent than the mining limit (GetResourceUsers < mining limit; the
-- port counts the gatherers sent there, EECPNextAssigned)
local function has_room(memory, u)
    return M.gatherers(u.id) < T.b(memory, "mining_limit")
end

-- RefreshResources: the huntable animals (food, family animal, no attack) sorted by distance from the town, and the
-- fish; explored only (DEVIATION, fog: the original lists nature's every unit)
function M.refresh_resources(view, memory)
    local rm = M.state(memory)
    local tx, ty = town(view)
    rm.mines, rm.fish = {}, {}
    for _, id in ipairs(ai.resources{kind = "food"}) do
        local u = M.unit(id)
        if u ~= nil and u.type ~= C.forage_bush then
            local c = class_of(u.type)
            if c.family == FAMILY_FISH or u.type == C.fish then
                rm.fish[#rm.fish + 1] = id
            elseif c.family == FAMILY_ANIMAL and (c.attack or 0) == 0 then
                rm.mines[#rm.mines + 1] = id
            end
        end
    end
    if tx ~= nil then
        local dist = {}
        for _, id in ipairs(rm.mines) do
            local u = M.unit(id)
            dist[id] = u ~= nil and d2(u.x, u.y, tx, ty) or 1 << 40
        end
        table.sort(rm.mines, function(a, b)
            if dist[a] ~= dist[b] then
                return dist[a] < dist[b]
            end
            return a < b
        end)
    end
end

-- FindMines(n, near): live animals on the town's land within 2 (near) or 6 drop-off distances of a town centre
function M.find_mines(view, memory, n, near)
    local rm = M.state(memory)
    local out = {}
    local tx, ty = town(view)
    local r = T.b(memory, "drop_off_distance")
    local range = near and r * r * 4 or r * r * 36
    local centres = {}
    for _, d in ipairs(M.drops(false)) do
        if d.built then
            centres[#centres + 1] = d
        end
    end
    for _, id in ipairs(rm.mines) do
        if #out >= n then
            break
        end
        local u = M.unit(id)
        if u ~= nil and (u.amount or 0) > 0 and (tx == nil or ai.reach(tx, ty, u.x, u.y)) and not M.enemy_near(u.x, u.y)
            and not M.herd_banned(memory, u.x, u.y) then
            local best
            for _, d in ipairs(centres) do
                local dd = d2(d.x, d.y, u.x, u.y)
                if best == nil or dd < best then
                    best = dd
                end
            end
            if best ~= nil and best < range then
                out[#out + 1] = u
            end
        end
    end
    return out
end

-- FindFarm(needs_drop, near, unit): the first forage bush of a centre with room, on the unit's land (else the town's),
-- explored, with something left, not by enemies, within the claim range (economy.forage_patches), within 2 (near) or
-- 6 drop-off distances of the town centre
function M.find_farm(view, memory, needs_drop, near, unit)
    local rm = M.state(memory)
    local tx, ty = town(view)
    local r = T.b(memory, "drop_off_distance")
    local range = near and r * r * 4 or r * r * 36
    if near and tx == nil then
        return nil
    end
    local claim = memory.t.personality.claim_range
    for _, c in ipairs(rm.centres) do
        local u = find_resource_unit(c, "food")
        if u ~= nil and u.type == C.forage_bush then
            local ok = true
            if unit ~= nil then
                ok = ai.reach(unit.x, unit.y, u.x, u.y)
            elseif tx ~= nil then
                ok = ai.reach(tx, ty, u.x, u.y)
            end
            ok = ok and has_room(memory, u)
            if ok and needs_drop and not M.drop_near(u.x, u.y, r, true) then
                ok = false   -- (inNeedsDropSite: the centre has a drop site)
            end
            ok = ok and not M.enemy_near(u.x, u.y)
            if ok and claim and not M.in_claim(view, memory, u.x, u.y) then
                ok = false
            end
            if ok and (tx == nil or d2(u.x, u.y, tx, ty) < range) then
                return u
            end
        end
    end
    return nil
end

-- FindResourceTarget(r, unit, backwards): the active centres' first unit of the resource with room (from the last
-- centre with backwards), explored, not by enemies or in a danger zone, not within 10 tiles of another player's land
-- (DEVIATION: the port skips the other players' area hull test; its enemy test is fog-honest), on the unit's land;
-- with the claim range, a first pass inside it
function M.find_resource_target(view, memory, r, unit, backwards, claim_pass)
    local rm = M.state(memory)
    if not claim_pass and memory.t.personality.claim_range then
        local near = M.find_resource_target(view, memory, r, unit, backwards, true)
        if near ~= nil then
            return near
        end
    end
    local n = #rm.centres
    for k = 1, n do
        local c = rm.centres[backwards and n - k + 1 or k]
        if c.active then
            local u
            -- a unit with room (the first with room, else the first)
            for _, id in ipairs(c.u) do
                local v = M.unit(id)
                if v ~= nil and v.kind == r and (v.amount or 0) > 0 then
                    if has_room(memory, v) then
                        u = v
                        break
                    end
                end
            end
            if u ~= nil and not M.enemy_near(u.x, u.y) and not M.danger_at(memory, u.x, u.y)
                and (not claim_pass or M.in_claim(view, memory, u.x, u.y))
                and (unit == nil or ai.reach(unit.x, unit.y, u.x, u.y)) then
                return u
            end
        end
    end
    return nil
end

-- FindForestTarget: a tree of the forest nearest the town centre on the unit's land. DEVIATION: the port takes the
-- explored tree nearest the town (the original walks the continent manager's forests nearest first, single trees
-- only for fewer than 8 gatherers)
function M.find_forest_target(view, memory, count, unit)
    local tx, ty = town(view)
    if tx == nil then
        return nil
    end
    for _, id in ipairs(ai.resources{kind = "wood", near = {tx, ty, 60}, limit = 12}) do
        local u = M.unit(id)
        if u ~= nil and (unit == nil or ai.reach(unit.x, unit.y, u.x, u.y)) then
            return u
        end
    end
    return nil
end

-- the own docks
local function docks()
    local out = {}
    for _, id in ipairs(ai.units{type = C.dock, built = true}) do
        local u = M.unit(id)
        if u ~= nil then
            out[#out + 1] = u
        end
    end
    return out
end

-- FindNearestDock / FindFish: the fish nearest the dock with room and no dock within 8 tiles of it first, else any
-- with room
function M.find_fish(memory, dock)
    local rm = M.state(memory)
    local list = {}
    for _, id in ipairs(rm.fish) do
        local u = M.unit(id)
        if u ~= nil and (u.amount or 0) > 0 then
            list[#list + 1] = u
        end
    end
    table.sort(list, function(a, b)
        local da, db = d2(a.x, a.y, dock.x, dock.y), d2(b.x, b.y, dock.x, dock.y)
        if da ~= db then
            return da < db
        end
        return a.id < b.id
    end)
    local ds = docks()
    for _, u in ipairs(list) do
        if has_room(memory, u) then
            local free = true
            for _, d in ipairs(ds) do
                if d2(d.x, d.y, u.x, u.y) <= 64 then
                    free = false
                end
            end
            if free then
                return u
            end
        end
    end
    for _, u in ipairs(list) do
        if has_room(memory, u) then
            return u
        end
    end
    return nil
end

function M.nearest_dock(unit)
    local best, bd
    for _, d in ipairs(docks()) do
        local dd = d2(d.x, d.y, unit.x, unit.y)
        if bd == nil or dd < bd then
            best, bd = d, dd
        end
    end
    return best
end

-- the farms: GetIdleFarms (built, nobody working) / GetUnfinishedFarms
function M.farms(memory, built, max)
    local out = {}
    local farm = memory.civ ~= nil and memory.civ.farm_type or C.farm
    for _, id in ipairs(ai.units{type = farm}) do
        if #out >= max then
            break
        end
        local u = M.unit(id)
        if u ~= nil and u.built == built then
            if not built or M.gatherers(u.id) == 0 then
                out[#out + 1] = u
            end
        end
    end
    return out
end

-- ---------------------------------------------------------------------------------------------------------------
-- the danger zones (EECPRMNoteDanger / EECPRMResourceInDanger; the personality's militia or a human's hits): the
-- port reads them from its own hits (main.lua notes them): a tile within kDangerRange 10 of a hit in the last 30 s

function M.danger_at(memory, x, y)
    local zones = memory.rm ~= nil and memory.rm.danger or nil
    if zones == nil then
        return false
    end
    for _, z in ipairs(zones) do
        if d2(z.x, z.y, x, y) <= 100 then
            return true
        end
    end
    return false
end

-- the herds given up (DEVIATION, fog: the port sees fewer herds near home and hunts far ones under the enemy's towers,
-- where the engine carries its hunters on from animal to animal; 145 citizens killed by towers in 32 games against
-- the C++'s 40): where an enemy tower killed a hunter, no game within 12 tiles is hunted for 10 minutes, and the
-- hunters there leave it (AI_RESEARCH 8.61)
M.HERD_BAN_RANGE, M.HERD_BAN_MS = 12, 600000

function M.herd_banned(memory, x, y)
    local bans = memory.rm ~= nil and memory.rm.herd_bans or nil
    if bans == nil then
        return false
    end
    for _, b in ipairs(bans) do
        if d2(b.x, b.y, x, y) <= M.HERD_BAN_RANGE * M.HERD_BAN_RANGE then
            return true
        end
    end
    return false
end

function M.is_animal(id)
    local u = id ~= nil and M.unit(id) or nil
    return u ~= nil and class_of(u.type).family == FAMILY_ANIMAL
end

-- ---------------------------------------------------------------------------------------------------------------
-- SendToGather: an idle citizen (or one already gathering) to a resource; on the lists under its resource

function M.send_to_gather(memory, id, res)
    local rm = M.state(memory)
    local code = ai.gather({id}, res.id)
    M.touch(id)
    if code ~= "ok" then
        return false
    end
    M.add_gatherers(res.id, 1)
    M.remove_gatherer(rm, id)
    M.add_gatherer(rm, id, res.kind)
    return true
end

return M

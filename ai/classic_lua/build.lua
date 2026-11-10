-- EECPBuildManager in Lua (the classic port; ORIGIN in this folder): the building requests in per-epoch FIFOs, one
-- started every 3 s (up to 4 tried), the site finders (the resource centres' rings, the granary, the shore, the
-- towers at the worked centres), the builders (the nearest available citizen through ai.build), the builders checked
-- every 2 minutes. require("classic_lua.build").
--
-- Memory (memory.bm): q[epoch + 1] = {{class = name, x, y}}, the timers.
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local C = T.C

local M = {}

local d2 = R.d2

function M.state(memory)
    local bm = memory.bm
    if bm == nil then
        bm = {q = {}, next_update = 0, next_builder_check = 0, sites = {}}
        for e = 1, 15 do
            bm.q[e] = {}
        end
        memory.bm = bm
    end
    return bm
end

local function U()
    return require("classic_lua.units")
end

-- IsQueued: a class in any epoch's queue
function M.is_queued(memory, class)
    local bm = M.state(memory)
    for e = 1, 15 do
        for _, q in ipairs(bm.q[e]) do
            if q.class == class then
                return true
            end
        end
    end
    return false
end

function M.has_later_requests(view, memory)
    local bm = M.state(memory)
    for e = view.epoch + 2, 15 do
        if #bm.q[e] > 0 then
            return true
        end
    end
    return false
end

-- QueueBuilding(class, append, front): in the class's epoch queue (append: always; else once, and not while one is
-- unfinished, at the front or the back), its cost into the needs. The personality's vetoes (the C++ classic's, for
-- the classic engine only): no houses in the Prehistoric with the claim range; before the Copper Age at most
-- early_trainers barracks and archery ranges
function M.queue(view, memory, class, append, front, x, y)
    local p = memory.t.personality
    if class == C.house and p.claim_range and view.epoch <= 0 then
        return
    end
    local trainers = p.early_trainers or 0
    if trainers > 0 and view.epoch < 2 and (class == C.barracks or class == C.archery_range) then
        if #ai.units{type = class} >= trainers then
            return
        end
    end
    local bm = M.state(memory)
    local t = U().class_tech(view, class)
    if t == nil then
        return
    end
    local list = bm.q[t.epoch + 1]
    if list == nil then
        return
    end
    local item = {class = class, x = x or 0, y = y or 0}
    if append or x ~= nil then
        list[#list + 1] = item
    else
        for _, q in ipairs(list) do
            if q.class == class then
                return
            end
        end
        if class ~= C.farm and class ~= C.robotic_farm and U().has_unfinished(class) then
            return
        end
        if front then
            table.insert(list, 1, item)
        else
            list[#list + 1] = item
        end
    end
    R.add_costs(memory, U().cost(class), 1)
    print(string.format("classic_lua build.queue %s e=%d", class, t.epoch))
end

-- DropObsoleteRequests
function M.drop_obsolete(view, memory, epoch)
    local bm = M.state(memory)
    for e = 1, epoch do
        local list = bm.q[e]
        for k = #list, 1, -1 do
            local t = U().class_tech(view, list[k].class)
            if t ~= nil and t.obsolete_epoch < epoch then
                table.remove(list, k)
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the builders (IsCitizenAvailable, FindBuilder)

-- the citizens free for a job: idle or gathering, on the map
function M.available_citizens()
    return R.memo("available_citizens", M.available_citizens_now)
end

-- (the builder ordered leaves the step's list)
local function used(id)
    local list = M.available_citizens()
    for k = #list, 1, -1 do
        if list[k].id == id then
            table.remove(list, k)
        end
    end
    R.touch(id)
end
M.used = used

function M.available_citizens_now()
    local out = {}
    for _, id in ipairs(ai.units{category = "citizen"}) do
        local u = R.unit(id)
        if u ~= nil and u.inside == nil and (u.action == "idle" or u.action == "gather") then
            out[#out + 1] = u
        end
    end
    return out
end

-- FindBuilder: the nearest available citizen on the site's land; a farm's builder is not a farmer
function M.find_builder(memory, x, y, class)
    local best, bd
    local farm = memory.civ ~= nil and memory.civ.farm_type or C.farm
    for _, u in ipairs(M.available_citizens()) do
        local dd = d2(u.x, u.y, x, y)
        if (bd == nil or dd < bd or (dd == bd and u.id < best.id)) and ai.reach(u.x, u.y, x, y) then
            local skip = false
            if class == farm and u.action == "gather" and u.target ~= nil then
                local t = R.unit(u.target)
                skip = t ~= nil and t.type == farm
            end
            if not skip then
                best, bd = u, dd
            end
        end
    end
    return best
end

-- ---------------------------------------------------------------------------------------------------------------
-- the site finders (FindSite and its finders). DEVIATION: the port places through ai.find_site (EECanPlaceOnTile,
-- with the classic's one-tile margin where CanBuildAt has it); the classic's CanBuildAt also keeps off choke points,
-- hills, runway ends, gates and town centres within 3 tiles and asks for walkable ground round the site

local function radius(memory)
    return T.b(memory, "drop_off_distance")
end

-- a centre's drop site (mUnit): an own town centre, capitol or settlement within the drop-off distance of one of its
-- units (AdoptDropSite)
function M.centre_drop(memory, c)
    local r = radius(memory)
    if c.town then
        -- the town centre's own centre (Regroup's): its drop site is the town centre (or what stands there now)
        for _, d in ipairs(R.drops(false)) do
            if d2(d.x, d.y, c.x, c.y) <= r * r then
                return d
            end
        end
        return nil
    end
    for _, d in ipairs(R.drops(false)) do
        for _, id in ipairs(c.u) do
            local u = R.unit(id)
            if u ~= nil and d2(u.x, u.y, d.x, d.y) <= r * r then
                return d
            end
        end
    end
    return nil
end

-- the centres nearest the town centre first (EECPBMNearestHomeFirst)
local function nearest_home_first(view, list)
    local tx, ty = R.town(view)
    if tx == nil then
        return list
    end
    table.sort(list, function(a, b)
        local da, db = d2(a.x, a.y, tx, ty), d2(b.x, b.y, tx, ty)
        if da ~= db then
            return da < db
        end
        return (a.u[1] or 0) < (b.u[1] or 0)
    end)
    return list
end

-- FindBuildSite(settlement): rings 2 .. the drop-off distance round the centre's middle, with a margin
local function centre_settlement_site(memory, c)
    local x, y = ai.find_site(C.settlement, c.x, c.y, {min = 2, max = math.min(30, radius(memory) - 1), margin = true})
    return x, y
end

-- FindBuildSite(production): from the drop-off distance + 5 inward to 2 round the centre's middle (the town centre
-- when it has no units), with a margin
local function centre_production_site(view, memory, c, class)
    local cx, cy = c.x, c.y
    if #c.u == 0 then
        cx, cy = R.town(view)
    end
    if cx == nil then
        return nil
    end
    return ai.find_site(class, cx, cy, {min = 2, max = math.min(30, radius(memory) + 5), far = true, margin = true})
end

-- the own towers of the epoch's class within a squared distance of a tile (HasTowerNear)
function M.tower_near(view, x, y, r2)
    for _, id in ipairs(ai.units{type = T.tower_class(view.epoch)}) do
        local u = R.unit(id)
        if u ~= nil and d2(u.x, u.y, x, y) <= r2 then
            return true
        end
    end
    return false
end

-- FindBuildSite(tower): by a worked centre's gold and iron (and its food while active: the port's rule of the C++
-- classic), in the rings 2 .. the tower's range, not within the tower spacing of another tower
local function centre_tower_site(view, memory, c, class)
    local cls = ai.class(class)
    local range = cls ~= nil and (cls.range or 0) // 100 or 0
    if range < 3 then
        range = 3
    end
    local spacing = T.b(memory, "tower_spacing_k") * range // 1000
    for _, id in ipairs(c.u) do
        local u = R.unit(id)
        if u ~= nil and (u.kind == "gold" or u.kind == "iron" or (u.kind == "food" and c.active)) then
            local x, y = ai.find_site(class, u.x, u.y, {min = 2, max = range, margin = true})
            if x ~= nil and not M.tower_near(view, x, y, spacing * spacing) then
                return x, y
            end
        end
    end
    return nil
end

-- FindTowerSite: the centres with a drop site, the active ones nearest the town first (island coasts and the gates'
-- and choke points' towers: not ported)
local function find_tower_site(view, memory, class)
    local rm = R.state(memory)
    local tx, ty = R.town(view)
    if tx == nil then
        return nil
    end
    local order = {}
    for _, c in ipairs(rm.centres) do
        local d = M.centre_drop(memory, c)
        if d ~= nil then
            order[#order + 1] = {c = c, key = (c.active and 0 or (1 << 40)) + d2(d.x, d.y, tx, ty)}
        end
    end
    table.sort(order, function(a, b)
        if a.key ~= b.key then
            return a.key < b.key
        end
        return (a.c.u[1] or 0) < (b.c.u[1] or 0)
    end)
    for _, o in ipairs(order) do
        local x, y = centre_tower_site(view, memory, o.c, class)
        if x ~= nil then
            return x, y
        end
    end
    return nil
end

-- FindGranarySite: just beyond a centre's drop-off distance (within the claim range with the personality's claim
-- range, nearest home first). DEVIATION: the room for the farms round it is not tested
local function find_granary_site(view, memory)
    local rm = R.state(memory)
    local r = radius(memory)
    local claim = memory.t.personality.claim_range
    local list = {}
    local tx, ty = R.town(view)
    for _, c in ipairs(rm.centres) do
        if not claim or tx == nil or d2(c.x, c.y, tx, ty) <= R.claim_range(memory) * R.claim_range(memory) then
            list[#list + 1] = c
        end
    end
    if claim then
        nearest_home_first(view, list)
    end
    for _, c in ipairs(list) do
        local x, y = ai.find_site(C.granary, c.x, c.y, {min = r + 1, max = math.min(30, r + 29), margin = true})
        if x ~= nil and (not claim or tx == nil or d2(x, y, tx, ty) <= R.claim_range(memory) * R.claim_range(memory)) then
            return x, y
        end
    end
    return nil
end

-- FindCenterSite: a settlement at a centre without a drop site; a temple, university or hospital at one with a drop
-- site (those within 20 tiles of the town first). DEVIATION: the area effects' order is not ported
local function find_center_site(view, memory, class)
    local rm = R.state(memory)
    if class == C.settlement then
        for _, c in ipairs(rm.centres) do
            if M.centre_drop(memory, c) == nil and #c.u > 0 then
                local x, y = centre_settlement_site(memory, c)
                if x ~= nil then
                    return x, y
                end
            end
        end
        return nil
    end
    local list = {}
    for _, c in ipairs(rm.centres) do
        if M.centre_drop(memory, c) ~= nil then
            list[#list + 1] = c
        end
    end
    nearest_home_first(view, list)
    for _, c in ipairs(list) do
        local x, y = ai.find_site(class, c.x, c.y, {min = 2, max = math.min(30, radius(memory) + 5), margin = true})
        if x ~= nil then
            return x, y
        end
    end
    return nil
end

-- FindProductionSite: the active centres nearest the town centre first
local function find_production_site(view, memory, class)
    local rm = R.state(memory)
    local list = {}
    for _, c in ipairs(rm.centres) do
        if c.active and M.centre_drop(memory, c) ~= nil then
            list[#list + 1] = c
        end
    end
    nearest_home_first(view, list)
    for _, c in ipairs(list) do
        local x, y = centre_production_site(view, memory, c, class)
        if x ~= nil then
            return x, y
        end
    end
    -- (the town centre's own centre when no centre has a drop site yet)
    local tx, ty = R.town(view)
    if tx ~= nil then
        return ai.find_site(class, tx, ty, {min = 2, max = math.min(30, radius(memory) + 5), far = true, margin = true})
    end
    return nil
end

-- FindShoreSite: a dock site on the water nearest the town, 8+ tiles from the other docks. DEVIATION: the classic
-- collects the water site tiles of the largest water next to the town's land (CollectSiteTiles); the port takes the
-- shore nearest the town (ai.shore) and the free site there
local function find_shore_site(view, memory, class)
    local tx, ty = R.town(view)
    if tx == nil then
        return nil
    end
    local sx, sy = ai.shore(tx, ty, {max = 30})
    if sx == nil then
        return nil
    end
    local x, y = ai.find_site(class, sx, sy, {min = 0, max = 6})
    if x == nil then
        return nil
    end
    for _, t in ipairs({C.dock, C.navy_yard}) do
        for _, id in ipairs(ai.units{type = t}) do
            local u = R.unit(id)
            if u ~= nil and d2(u.x, u.y, x, y) <= 75 then
                return nil
            end
        end
    end
    return x, y
end

-- the house site (EECPResourceCenter::FindBuildSite(house) through QueueMoraleBuilding): next to the centre's drop
-- site within its morale radius. DEVIATION: the morale effect's area is approximated by 8 tiles round the drop site
function M.house_site(memory, d)
    return ai.find_site(C.house, d.x, d.y, {min = d.size // 2 + 2, max = 8, margin = true})
end

-- FindSite
function M.find_site(view, memory, class)
    if class == C.granary then
        return find_granary_site(view, memory)
    elseif class == C.dock or class == C.navy_yard then
        return find_shore_site(view, memory, class)
    elseif class == C.temple or class == C.settlement or class == C.university or class == C.hospital then
        return find_center_site(view, memory, class)
    elseif class == T.tower_class(view.epoch) or class == C.priest_tower then
        return find_tower_site(view, memory, class)
    elseif class == C.barracks or class == C.archery_range or class == C.siege_factory or class == C.stable
        or class == C.tank_factory or class == C.cyber_factory or class == C.cyber_lab then
        return find_production_site(view, memory, class)
    elseif class == C.farm or class == C.robotic_farm then
        -- an unbuilt farm foundation of the granary
        for _, id in ipairs(ai.units{type = class}) do
            local u = R.unit(id)
            if u ~= nil and not u.built and u.hp == 1 then
                return u.x, u.y, id
            end
        end
        return nil
    end
    -- (airports, anti-air, space and wonder sites: not ported; the town centre's rings)
    local tx, ty = R.town(view)
    if tx == nil then
        return nil
    end
    return ai.find_site(class, tx, ty, {min = 3, max = 20, margin = true})
end

-- ---------------------------------------------------------------------------------------------------------------
-- StartBuilding: a site and a builder; -> whether the request is done

local DROP_WITHOUT_BUILDER = {}

local function start_building(view, memory, req)
    local class = req.class
    if (class:find("^b_wall") ~= nil) then
        return true   -- walls go to the wall manager (not ported yet)
    end
    local avail = M.available_citizens()
    if #avail == 0 then
        return false
    end
    local x, y, fid = req.x, req.y, nil
    if x == 0 and y == 0 then
        x, y, fid = M.find_site(view, memory, class)
    end
    if x == nil then
        -- no site: with the claim range a farm, granary or settlement claims more land
        if memory.t.personality.claim_range and (class == C.farm or class == C.granary or class == C.settlement) then
            R.claim_expand(view, memory)
        end
        return false
    end
    local w = M.find_builder(memory, x, y, class)
    if w ~= nil then
        used(w.id)
        if fid ~= nil then
            ai.repair({w.id}, fid)
        else
            local code, reason, n, id = ai.build({w.id}, class, x, y)
            if code ~= "ok" then
                return false
            end
        end
        return true
    end
    -- no builder: these classes are dropped (the original's list)
    local rm = memory
    return class == C.temple or class == C.university or class == C.hospital or class == C.house or class == C.farm
        or class == C.robotic_farm or class == C.granary or class == C.priest_tower
        or (class == C.dock and #ai.units{type = C.dock} > 0) or (class == C.navy_yard and #ai.units{type = C.navy_yard} > 0)
        or (class == C.airport and #ai.units{type = C.airport} > 0)
end

-- AssignBuilders: every unfinished own building has a builder (a citizen building it), else the nearest available one
local function assign_builders(view, memory)
    local building = {}
    for _, id in ipairs(ai.units{category = "citizen"}) do
        local u = R.unit(id)
        if u ~= nil and u.action == "build" and u.target ~= nil then
            building[u.target] = true
        end
    end
    for _, id in ipairs(ai.units{category = "building", built = false}) do
        local b = R.unit(id)
        if b ~= nil and not b.pending and not building[id] and not R.enemy_near(b.x, b.y) then
            local w = M.find_builder(memory, b.x, b.y, b.type)
            if w ~= nil then
                used(w.id)
                ai.repair({w.id}, id)
                building[id] = true
            end
        end
    end
end

-- Update: every 3 s the first buildable request of each epoch to the current one (up to 4 tried; one started),
-- the builders every 2 minutes
function M.update(view, memory)
    local bm = M.state(memory)
    if view.time <= bm.next_update then
        return
    end
    local tried, started = 0, false
    for e = 1, view.epoch + 1 do
        if started or tried >= 4 then
            break
        end
        local list = bm.q[e]
        local left = #list
        while left > 0 and not started and tried < 4 do
            left = left - 1
            local req = table.remove(list, 1)
            local t = U().class_tech(view, req.class)
            if t ~= nil and t.obsolete then
                -- dropped
            elseif t == nil or not t.producible
                or not U().can_afford(view, memory, U().cost(req.class), 1, req.class) then
                -- (CanProduceUnitType: producible and affordable, the classic's CanAfford)
                list[#list + 1] = req
                break
            else
                tried = tried + 1
                if start_building(view, memory, req) then
                    print(string.format("classic_lua build.start %s", req.class))
                    R.remove_costs(memory, U().cost(req.class), 1)
                    started = true
                else
                    list[#list + 1] = req
                end
            end
        end
    end
    if view.time > bm.next_builder_check then
        if bm.next_builder_check > 0 then
            assign_builders(view, memory)
        end
        bm.next_builder_check = view.time + 120000
    end
    bm.next_update = view.time + 3000
end

return M

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
    R.add_costs(memory, U().ledger_cost(class), 1)
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
        if u ~= nil and u.inside == nil and (u.action == "idle" or u.action == "gather")
                and not require("classic_lua.ferry").busy_now[id] then
            out[#out + 1] = u
        end
    end
    return out
end

-- FindBuilder's shore rule: for the site classes (EECPIsSiteClass: the dock, the navy yard, the space classes) and
-- the tower class a citizen of another path area qualifies when its continent's shore takes in a tile of the
-- footprint (a dock's water site lies on no land of the town: the shallows are a land mass of their own). The port:
-- the land masses by the footprint (its tiles and the ring round it: ai.shore with max 0, a land tile by water), one
-- tile of each; a citizen qualifies when it reaches one of them (cached for the step per citizen and land mass).
-- DEVIATION: the ring round the footprint stands in for the continent's shore tiles
local SITE_CLASSES = {[C.dock] = true, [C.navy_yard] = true, [C.space_dock] = true, b_space_turret = true,
                      w_orbital_space_station = true}

local function shore_class(class)
    if SITE_CLASSES[class] then
        return true
    end
    for e = 0, 15 do
        if T.tower_class(e) == class then
            return true
        end
    end
    return false
end

local function footprint_lands(x, y, class)
    return R.memo("footprint_" .. class .. "_" .. x .. "_" .. y, function()
        local size = R.class_of(class).size or 1
        if size < 1 then
            size = 1
        end
        local out, seen = {}, {}
        for ty = y - 1, y + size do
            for tx = x - 1, x + size do
                local lx, ly, _, mass = ai.shore(tx, ty, {max = 0})
                if lx ~= nil and not seen[mass] then
                    seen[mass] = true
                    out[#out + 1] = {lx, ly, mass}
                end
            end
        end
        return out
    end)
end

local function reaches(u, x, y, lands)
    if ai.reach(u.x, u.y, x, y) then
        return true
    end
    local cache = R.memo("shore_reach", function() return {} end)
    for _, l in ipairs(lands) do
        local key = u.id .. ":" .. l[3]
        local r = cache[key]
        if r == nil then
            r = ai.reach(u.x, u.y, l[1], l[2])
            cache[key] = r
        end
        if r then
            return true
        end
    end
    return false
end

-- FindBuilder: the nearest available citizen on the site's land (or, for the site classes and towers, by its
-- shore); a farm's builder is not a farmer
function M.find_builder(memory, x, y, class)
    local best, bd
    local farm = memory.civ ~= nil and memory.civ.farm_type or C.farm
    local lands = shore_class(class) and footprint_lands(x, y, class) or nil
    for _, u in ipairs(M.available_citizens()) do
        local dd = d2(u.x, u.y, x, y)
        if (bd == nil or dd < bd or (dd == bd and u.id < best.id))
                and (lands ~= nil and reaches(u, x, y, lands) or lands == nil and ai.reach(u.x, u.y, x, y)) then
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
-- classic), ring by ring from 2 to below the tower's range, a site not within the tower spacing of another tower and
-- with no enemy near (CanBuildAt's HasEnemyNear: ai.enemy_near). DEVIATION: the first placeable tile of each ring
-- (the classic tries every tile of the ring, nearest the drop site first, skipping the nearest three)
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
            for ring = 2, range - 1 do
                local x, y = ai.find_site(class, u.x, u.y, {min = ring, max = ring, margin = true})
                if x ~= nil and not M.tower_near(view, x, y, spacing * spacing) and not R.enemy_near(x, y) then
                    return x, y
                end
            end
        end
    end
    return nil
end

-- FindTowerSite's other sites. The tower's reach: 1.5 x its range. DEVIATIONS: a town is a drop site (town centre,
-- capitol, settlement); a coast is the largest water's shore by it (ai.water_sites; the classic walks the town's
-- continent's shore tiles); the gate's and choke point's site is the first placeable tile (ai.find_site), not a
-- random one
local function tower_reach2(class)
    local cls = ai.class(class)
    local range = cls ~= nil and (cls.range or 0) // 100 or 0
    if range < 3 then
        range = 3
    end
    return (range * 3 // 2) * (range * 3 // 2), range
end

local function towns_nearest_first(view)
    local tx, ty = R.town(view)
    local list = {}
    for _, d in ipairs(R.drops(false)) do
        if d.built then
            list[#list + 1] = d
        end
    end
    table.sort(list, function(a, b)
        local da, db = d2(a.x, a.y, tx, ty), d2(b.x, b.y, tx, ty)
        if da ~= db then
            return da < db
        end
        return a.id < b.id
    end)
    return list
end

-- the island map's coasts: the shore by each town, nearest the town first, a site in the rings 1-2 round a shore tile
-- with no tower within reach
local function coast_tower_site(view, memory, class)
    if not memory.t.island then
        return nil
    end
    local reach2 = tower_reach2(class)
    local asked = 0
    for _, d in ipairs(towns_nearest_first(view)) do
        for _, st in ipairs(ai.water_sites(d.x, d.y, {max = 24})) do
            if not M.tower_near(view, st[1], st[2], reach2) then
                local x, y = ai.find_site(class, st[1], st[2], {min = 1, max = 2})
                if x ~= nil then
                    return x, y
                end
                asked = asked + 1
                if asked >= 12 then
                    return nil
                end
            end
        end
    end
    return nil
end

-- the gates nearest the town first: a site within 5 tiles of a gate with no tower within reach, off the gate (2) and
-- the wall (4)
local function gate_tower_site(view, memory, class)
    local reach2 = tower_reach2(class)
    local tx, ty = R.town(view)
    local gates = {}
    for _, id in ipairs(ai.units{category = "building", built = true}) do
        local u = R.unit(id)
        if u ~= nil and u.type:find("gate") ~= nil then
            gates[#gates + 1] = u
        end
    end
    table.sort(gates, function(a, b)
        local da, db = d2(a.x, a.y, tx, ty), d2(b.x, b.y, tx, ty)
        if da ~= db then
            return da < db
        end
        return a.id < b.id
    end)
    for _, g in ipairs(gates) do
        if not M.tower_near(view, g.x, g.y, reach2) then
            local x, y = ai.find_site(class, g.x, g.y, {min = 3, max = 5})
            if x ~= nil then
                return x, y
            end
        end
    end
    return nil
end

-- the choke points of the town's land within 7 tiles of a town, none of the class within twice its range: the
-- choke point's tiles nearest the town first
local function choke_tower_site(view, memory, class)
    if ai.choke_point == nil then
        return nil
    end
    local _, range = tower_reach2(class)
    for _, d in ipairs(towns_nearest_first(view)) do
        local cx, cy = ai.choke_point(d.x, d.y, 1, 0, 1)
        if cx ~= nil and d2(cx, cy, d.x, d.y) < 50 and not M.tower_near(view, cx, cy, 4 * range * range) then
            local x, y = ai.find_site(class, cx, cy, {min = 0, max = 3})
            if x ~= nil then
                return x, y
            end
        end
    end
    return nil
end

-- FindTowerSite: an island map's coasts first; then the centres with a drop site, the active ones nearest the town
-- first; then the gates; then the choke points by the towns
local function find_tower_site(view, memory, class)
    local rm = R.state(memory)
    local tx, ty = R.town(view)
    if tx == nil then
        return nil
    end
    local cx, cy = coast_tower_site(view, memory, class)
    if cx ~= nil then
        return cx, cy
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
    local gx, gy = gate_tower_site(view, memory, class)
    if gx ~= nil then
        return gx, gy
    end
    return choke_tower_site(view, memory, class)
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

-- FindShoreSite: the first water site tile (CollectSiteTiles(2): the largest water body's tiles by the shore of the
-- town's land, nearest the town first; ai.water_sites) more than sqrt(75) tiles from every dock and navy yard, where
-- the dock can stand with no enemy near and a builder can reach it. DEVIATIONS (fog): the explored site tiles,
-- the 128 nearest, collected again every 60 s (the classic collects the whole map's once); CanBuildAt's strict rules are
-- ai.can_place's
local WATER_SITES_MS = 60000

local function water_sites(view, memory)
    local bm = M.state(memory)
    local tx, ty = R.town(view)
    if tx == nil then
        return {}
    end
    if bm.water_sites == nil or view.time - bm.water_sites_at >= WATER_SITES_MS then
        bm.water_sites = ai.water_sites(tx, ty, {max = 128})
        bm.water_sites_at = view.time
    end
    return bm.water_sites
end

local function find_shore_site(view, memory, class)
    local sites = water_sites(view, memory)
    if #sites == 0 then
        return nil
    end
    local docks = {}
    for _, t in ipairs({C.dock, C.navy_yard}) do
        for _, id in ipairs(ai.units{type = t}) do
            local u = R.unit(id)
            if u ~= nil then
                docks[#docks + 1] = u
            end
        end
    end
    local asked = 0
    for _, s in ipairs(sites) do
        local x, y = s[1], s[2]
        local near = false
        for _, u in ipairs(docks) do
            if d2(u.x, u.y, x, y) <= 75 then
                near = true
                break
            end
        end
        if not near and ai.can_place(class, x, y) and not R.enemy_near(x, y) then
            -- (the classic asks FindBuilderForTile at every site; the port at the first 48 placeable sites, for the
            -- step's budget)
            if M.find_builder(memory, x, y, class) ~= nil then
                return x, y
            end
            asked = asked + 1
            if asked >= 48 then
                return nil
            end
        end
    end
    return nil
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
-- CancelStalledBuilds: a builder waiting for its foundation's site to clear while another unit holds it (the record's
-- `stalled`): the foundation is deleted, its request put back at the front of its epoch's queue without a site (the
-- C++'s: FindSite picks another), the builder sent to the town centre; one builder at a time until none
local function cancel_stalled(view, memory)
    local bm = M.state(memory)
    local ids = ai.units{category = "citizen"}
    if #ids == 0 then
        return
    end
    local cols = ai.get(ids, {"stalled", "target"})
    local done = {}
    for k = 1, #ids do
        local target = cols.target[k]
        if cols.stalled[k] and target ~= nil and not done[target] then
            done[target] = true
            local b = R.unit(target)
            if b ~= nil then
                local t = U().class_tech(view, b.type)
                local e = t ~= nil and (t.epoch or 0) + 1 or 1
                bm.q[e] = bm.q[e] or {}
                table.insert(bm.q[e], 1, {class = b.type, x = 0, y = 0})
                ai.delete({target})
                local tx, ty = R.town(view)
                if tx ~= nil then
                    ai.move({ids[k]}, tx, ty)
                end
                print(string.format("classic_lua build.stalled %s at=%d,%d", b.type, b.x, b.y))
            end
        end
    end
end

function M.update(view, memory)
    local bm = M.state(memory)
    if view.time <= bm.next_update then
        return
    end
    cancel_stalled(view, memory)
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
                    R.remove_costs(memory, U().ledger_cost(req.class), 1)
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

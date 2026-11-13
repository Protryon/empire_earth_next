-- The classic's army in Lua (the classic port; ORIGIN in this folder): EECPCombatManager::Update's attack,
-- EECPAttackCommander's groups (CreateGroup, MakePlan, DropSpentGroups), EECPUnitSelector's composition
-- (ComputeGroupSize, the held, counter and favourite requirements), EECPPersonnel's requests (a slot per class: a free
-- unit of the class, else one ordered; the deadline), the sub-groups' life (wait for units, gather at the rally
-- point, the task: AssignLandTask's territory target, else EECPDefenseCommander::AssignSubGroupTask's ladder) and
-- EECPDefenseCommander with its EECPMilitaryAidPackages (AidOwnTowns, SendAid, SendBestSubGroup, CounterAttack,
-- PlanAid, CreateDefendGroup, GetUnitAllowance, ComputeTownThreat, Gather, Deploy, Recall).
--
-- A sub-group is a lib.groups group (its soldiers held by the script) with the classic's fields: kind (0 attack,
-- 1 defence), mission (0 none, 1 defend, 2 attack, 3 aid), the Personnel slots, the phase (wait, gather, task).
-- lib.groups moves and fights it: attack (a territory target, then what it meets), defend (a place and a radius).
-- require("classic_lua.army").
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local U = require("classic_lua.units")
local G = require("lib.groups")
local NAVY = require("lib.navy")
local FERRY = require("classic_lua.ferry")
local C = T.C

local M = {}

local REQUEST_MS = 600000        -- EECPPersonnelRequest's deadline
local GATHER_MS = 30000          -- EECPAttackSubGroup::HasGathered: the longest wait at the rally point
local DROP_MS = 45000            -- EECPAttackCommander::DropSpentGroups' period
local REGION = 9                 -- EEInfluenceTable's region size (tiles)
local ARMY_JOIN2 = 144           -- EECPIArmy::mJoinDistanceSq
local OWN_ARMY_RANGE2 = 225      -- kDCOwnArmyRange (and 1 / kDCOwnArmyFactor)
local DEPLOY_RANGE2 = 1200       -- MilitaryAidPackage::Deploy: groups this near the town's hull answer
local DEPLOY_NEAR2 = 144
local GUARD_RADIUS = 12          -- DEVIATION: a defence task's reach (the sub-group's target scan), lib.groups defend
local FOREST_RANGE = 35          -- AssignSubGroupTask: forests within squared distance 1225
local CITIZEN_DEATHS = 20        -- NoteThreat: more citizens lost in a region than this sends the army there

local KIND_SPECIAL, KIND_SIEGE, KIND_ARTILLERY, KIND_HERO, KIND_ANTI_TANK, KIND_ANTI_AIR = 3, 0x14, 0x10, 0x1b, 1, 0xd
local KIND_ANTI_SUB_HELICOPTER, KIND_MECH_ZEUS = 0x1c, 0x1e
local KIND_CARRIER = 0xf                -- kAIKindAircraftCarrier
local CARRIER = "s11_enterprise_aircraft_carrier"
local SPACE_CARRIER = "sp15_space_carrier"           -- kUnitTypeSp15SpaceCarrier   -- kUnitTypeS11EnterpriseAircraftCarrier
-- EECPUGatherCarrierFightersCount's carrier fighters (the three the data has)
local FIGHTERS = {"a_carrierfighter11_corsair1", "a_carrierfighter12_f_14", "a_carrierfighter13_avenger"}

local function army(memory)
    local a = memory.army
    if a == nil then
        a = {next_drop = 0, next_defense = 0, packages = {}, deaths = {}, damage = {}, hits = {}, files = {}}
        memory.army = a
    end
    return a
end

local function d2(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx * dx + dy * dy
end

local function shuffle(list)
    for k = #list, 2, -1 do
        local j = ai.random(1, k)
        list[k], list[j] = list[j], list[k]
    end
end

local function cls(name)
    return R.class_of(name)
end

-- ---------------------------------------------------------------------------------------------------------------
-- What is seen (EECPIntelligence, fog-honest: DEVIATION, the classic's files read every unit of a player)

-- the own military (CountMilitaryUnits' land units: soldiers and siege), records
local function own_military()
    return R.memo("army_own", function()
        local ids = ai.units{category = "soldier"}
        for _, id in ipairs(ai.units{category = "siege"}) do
            ids[#ids + 1] = id
        end
        local out = {}
        if #ids == 0 then
            return out
        end
        local cols = ai.get(ids, {"x", "y", "type", "held", "inside"})
        for k = 1, #ids do
            if cols.x[k] ~= nil then
                out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], type = cols.type[k], held = cols.held[k],
                                 inside = cols.inside[k]}
            end
        end
        return out
    end)
end

-- the own aircraft and helicopters (category "air"), records with their base (the airport or carrier they return to)
local function own_air()
    return R.memo("army_air", function()
        local ids = ai.units{category = "air"}
        local out = {}
        if #ids == 0 then
            return out
        end
        local cols = ai.get(ids, {"x", "y", "type", "held", "inside", "base"})
        for k = 1, #ids do
            if cols.x[k] ~= nil then
                out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], type = cols.type[k], held = cols.held[k],
                                 inside = cols.inside[k], base = cols.base[k]}
            end
        end
        return out
    end)
end

-- the own land strength (the file's land strength: the classes' influence)
local function own_strength()
    return R.memo("army_strength", function()
        local s = 0
        for _, u in ipairs(own_military()) do
            s = s + (cls(u.type).influence or 0)
        end
        return s
    end)
end

-- the own warships (the sea units of the classic's files: ships, not fishing boats or transports), records
local function own_ships()
    return R.memo("army_ships", function()
        local ids = ai.units{category = "ship"}
        local out = {}
        if #ids == 0 then
            return out
        end
        local cols = ai.get(ids, {"x", "y", "type", "held", "inside"})
        for k = 1, #ids do
            if cols.x[k] ~= nil then
                out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], type = cols.type[k], held = cols.held[k],
                                 inside = cols.inside[k]}
            end
        end
        return out
    end)
end

-- the free soldiers and warships (Personnel's pool: the engine holds them), {id -> record}, and a list in id order
local function free_pool()
    local pool, list = {}, {}
    for _, src in ipairs({own_military(), own_ships(), own_air()}) do
        for _, u in ipairs(src) do
            -- (an aircraft parked in its airport or carrier is free too)
            if u.held == "engine" and (u.inside == nil or src == own_air()) and (cls(u.type).speed or 0) > 0 then
                pool[u.id] = u
                list[#list + 1] = u
            end
        end
    end
    table.sort(list, function(p, q) return p.id < q.id end)
    return pool, list
end

-- a class's domain in the classic's files (AddUnitType): air, sea, space, else land
local function domain_of(c)
    local d = c.domain or "land"
    if d == "air" or d == "sea" or d == "space" then
        return d
    end
    return "land"
end

-- a unit the files count as military (AddUnitType: no citizen or boat; an attack, a calamity power, a medic or a
-- conversion)
local function file_military(c)
    return c.category ~= "citizen" and c.category ~= "boat" and ((c.attack or 0) > 0 or c.converts or c.medic)
end

-- the own file's strength and count per domain (EECPIntelligence's own player file: every own unit)
local function own_domains()
    return R.memo("army_domains", function()
        local out = {land = 0, sea = 0, air = 0, space = 0, n_land = 0, n_sea = 0, n_air = 0, n_space = 0}
        for _, src in ipairs({own_military(), own_ships(), own_air()}) do
            for _, u in ipairs(src) do
                local c = cls(u.type)
                if file_military(c) then
                    local d = domain_of(c)
                    out[d] = out[d] + (c.influence or 0)
                    out["n_" .. d] = out["n_" .. d] + 1
                end
            end
        end
        return out
    end)
end

-- an enemy player's file (EECPIntelligencePlayerFile::Update every 30 s): the military and gatherers known (the
-- most ever remembered at once, DEVIATION), the unit types of its military, its remembered land buildings
local function file_of(memory, player)
    local a = army(memory)
    local now = ai.time()
    local f = a.files[player]
    if f ~= nil and now < f.next then
        return f
    end
    f = f or {military = 0, gatherers = 0}
    local military, gatherers, types, buildings = 0, 0, {}, {}
    local dom = {land = 0, sea = 0, air = 0, space = 0}
    local seen_type = {}
    for _, m in ipairs(ai.memory{player = player}) do
        local c = cls(m.type)
        if m.building then
            if c.domain ~= "sea" then
                buildings[#buildings + 1] = {x = m.x, y = m.y, id = m.id, influence = c.influence or 0}
            end
        elseif c.category == "citizen" then
            gatherers = gatherers + 1
        elseif (c.attack or 0) > 0 or c.converts or c.medic then
            military = military + 1
            if c.category ~= "boat" then
                local d = domain_of(c)
                dom[d] = dom[d] + (c.influence or 0)
            end
            if not seen_type[m.type] then
                seen_type[m.type] = true
                types[#types + 1] = m.type
            end
        end
    end
    -- the strength per domain (the most remembered at once, as the counts)
    f.dom = f.dom or {land = 0, sea = 0, air = 0, space = 0}
    for _, d in ipairs({"land", "sea", "air", "space"}) do
        f.dom[d] = math.max(f.dom[d], dom[d])
    end
    -- with the manifest's sees_all the classic's own file: every unit of the player, seen or not (ai.player_file)
    local pf = memory.t.sees_all and ai.player_file ~= nil and ai.player_file(player) or nil
    if pf ~= nil then
        military = pf.military.land + pf.military.sea + pf.military.air + pf.military.space
        gatherers = pf.gatherers
        types = ai.keys(pf.types)
        f.military, f.gatherers = military, gatherers
        f.dom = {land = pf.strength.land, sea = pf.strength.sea, air = pf.strength.air, space = pf.strength.space}
        f.exact = true
    end
    table.sort(types)
    if not f.exact then
        f.military = math.max(f.military, military)
        f.gatherers = math.max(f.gatherers, gatherers)
    end
    f.types = types
    f.buildings = buildings
    f.next = now + 30000
    f.player = player
    a.files[player] = f
    return f
end

-- the file's strength: military x the military weight + gatherers x the worker weight. DEVIATION: what is known
-- of an enemy is less than the classic's omniscient count; the port takes each count at least at the own (the
-- own military and citizens), so a group is sized as against a mirror of the player when little is known
local function strength_of(view, memory, f)
    local b = memory.t.behavior
    local mil, gat = f.military, f.gatherers
    -- (the mirror only for a player known to have a town or buildings: one never seen with anything, such as a
    -- seat with no units, has the strength the classic's file gives it, nothing)
    if f.exact then
        return (mil * b.military_count_weight_k + gat * b.worker_count_weight_k) // 1000
    end
    local known = #(f.buildings or {}) > 0
    for _, e in ipairs(view.enemies or {}) do
        if e.player == f.player and (e.town_x or -1) >= 0 then
            known = true
        end
    end
    if known then
        mil = math.max(mil, #own_military())
        gat = math.max(gat, view.citizens or 0)
    end
    return (mil * b.military_count_weight_k + gat * b.worker_count_weight_k) // 1000
end

-- the enemy players alive
local function enemies()
    local humans, computers = {}, {}
    for _, p in ipairs(ai.players()) do
        if p.enemy and not p.me then   -- (the classic skips only observers: a player with no units left counts)
            if p.human then
                humans[#humans + 1] = p.id
            else
                computers[#computers + 1] = p.id
            end
        end
    end
    return humans, computers
end

-- EECPIntelligence::GetRandomEnemy
local function random_enemy()
    local humans, computers = enemies()
    for _, id in ipairs(computers) do
        humans[#humans + 1] = id
    end
    if #humans == 0 then
        return nil
    end
    return humans[ai.random(1, #humans)]
end

-- ---------------------------------------------------------------------------------------------------------------
-- Towns (EETown, EEPlayer::AddUnitToTown): the own land buildings (no walls) clustered, a building joining a town
-- whose hull (DEVIATION: the bounding box) it lies in or within 16 tiles of (mReach 256), towns it joins merged;
-- a town's id is its lowest building id, its value its building count

local TOWN_REACH2 = 256

local function towns()
    return R.memo("army_towns", function()
        local ids = ai.units{category = "building"}
        local out = {}
        if #ids == 0 then
            return out
        end
        local cols = ai.get(ids, {"x", "y", "type"})
        for k = 1, #ids do
            local x, y = cols.x[k], cols.y[k]
            local ty = cols.type[k]
            if x ~= nil and not ty:find("wall") and not ty:find("gate") then
                local joined = nil
                for _, tw in ipairs(out) do
                    if not tw.dead then
                        local px = math.max(tw.l, math.min(tw.r, x))
                        local py = math.max(tw.t, math.min(tw.b, y))
                        if d2(px, py, x, y) <= TOWN_REACH2 then
                            if joined == nil then
                                joined = tw
                            else
                                joined.l, joined.r = math.min(joined.l, tw.l), math.max(joined.r, tw.r)
                                joined.t, joined.b = math.min(joined.t, tw.t), math.max(joined.b, tw.b)
                                joined.value = joined.value + tw.value
                                joined.id = math.min(joined.id, tw.id)
                                tw.dead = true
                            end
                        end
                    end
                end
                if joined == nil then
                    out[#out + 1] = {id = ids[k], l = x, r = x, t = y, b = y, value = 1}
                else
                    joined.l, joined.r = math.min(joined.l, x), math.max(joined.r, x)
                    joined.t, joined.b = math.min(joined.t, y), math.max(joined.b, y)
                    joined.value = joined.value + 1
                    joined.id = math.min(joined.id, ids[k])
                end
            end
        end
        local keep = {}
        for _, tw in ipairs(out) do
            if not tw.dead then
                tw.cx, tw.cy = (tw.l + tw.r) // 2, (tw.t + tw.b) // 2
                keep[#keep + 1] = tw
            end
        end
        table.sort(keep, function(p, q) return p.id < q.id end)
        return keep
    end)
end

local function town_by_id(id)
    for _, tw in ipairs(towns()) do
        if tw.id == id then
            return tw
        end
    end
    return nil
end

local function hull_contains(tw, x, y)
    return x >= tw.l and x <= tw.r and y >= tw.t and y <= tw.b
end

local function hull_closest(tw, x, y)
    return math.max(tw.l, math.min(tw.r, x)), math.max(tw.t, math.min(tw.b, y))
end

-- the armies (EECPIArmy: land units with attack joined within 12 tiles of an army's bounds): {cx, cy, strength}
local function armies_of(units)
    local list = {}
    for _, u in ipairs(units) do
        local s = cls(u.type).influence or 0
        local joined = nil
        for _, a in ipairs(list) do
            local x = math.max(a.l, math.min(a.r, u.x))
            local y = math.max(a.t, math.min(a.b, u.y))
            if d2(x, y, u.x, u.y) <= ARMY_JOIN2 then
                if joined == nil then
                    joined = a
                else
                    -- merge into the first
                    joined.l, joined.r = math.min(joined.l, a.l), math.max(joined.r, a.r)
                    joined.t, joined.b = math.min(joined.t, a.t), math.max(joined.b, a.b)
                    joined.strength = joined.strength + a.strength
                    a.dead = true
                end
            end
        end
        if joined == nil then
            list[#list + 1] = {l = u.x, r = u.x, t = u.y, b = u.y, strength = s}
        else
            joined.l, joined.r = math.min(joined.l, u.x), math.max(joined.r, u.x)
            joined.t, joined.b = math.min(joined.t, u.y), math.max(joined.b, u.y)
            joined.strength = joined.strength + s
        end
        local keep = {}
        for _, a in ipairs(list) do
            if not a.dead then
                keep[#keep + 1] = a
            end
        end
        list = keep
    end
    for _, a in ipairs(list) do
        a.cx, a.cy = (a.l + a.r) // 2, (a.t + a.b) // 2
    end
    return list
end

-- the enemy armies seen (by player), and the strongest player's id by its armies' strength
local function enemy_armies()
    return R.memo("army_enemies", function()
        local byPlayer = {}
        local ids = ai.seen{enemy = true, armed = true}
        if #ids > 0 then
            local cols = ai.get(ids, {"x", "y", "type", "player", "category"})
            for k = 1, #ids do
                local c = cols.category[k]
                if cols.x[k] ~= nil and (c == "soldier" or c == "siege" or c == "hero") then
                    local p = cols.player[k]
                    byPlayer[p] = byPlayer[p] or {}
                    local l = byPlayer[p]
                    l[#l + 1] = {x = cols.x[k], y = cols.y[k], type = cols.type[k]}
                end
            end
        end
        local out = {}
        for _, p in ipairs(ai.keys(byPlayer)) do
            out[#out + 1] = {player = p, armies = armies_of(byPlayer[p])}
        end
        return out
    end)
end

local function own_armies()
    return R.memo("army_own_armies", function()
        return armies_of(own_military())
    end)
end

-- ComputeTownThreat: the threat (thousandths, 0..1000), the strongest enemy and, with fill, the threat points
-- {x, y, enemy, own} on the hull (the Hard range 55 tiles, Medium 30, Easy 15)
local function town_threat(memory, tw, fill)
    local range = T.by_difficulty(memory, 225, 900, 3025)
    local points = {}
    local function at(x, y)
        for _, p in ipairs(points) do
            if p.x == x and p.y == y then
                return p
            end
        end
        local p = {x = x, y = y, enemy = 0, own = 0}
        points[#points + 1] = p
        return p
    end
    local sum = 0
    local best, best_s = 0, -1
    for _, e in ipairs(enemy_armies()) do
        local total = 0
        for _, a in ipairs(e.armies) do
            total = total + a.strength
            if a.strength > 0 then
                if hull_contains(tw, a.cx, a.cy) then
                    sum = sum + a.strength * 2
                    if fill then
                        local p = at(hull_closest(tw, a.cx, a.cy))
                        p.enemy = p.enemy + a.strength * 3 // 2
                    end
                else
                    local px, py = hull_closest(tw, a.cx, a.cy)
                    local dist = d2(px, py, a.cx, a.cy)
                    if dist < range then
                        local v = dist * a.strength // range
                        sum = sum + v + a.strength
                        if fill then
                            local p = at(px, py)
                            p.enemy = p.enemy + v
                        end
                    end
                end
            end
        end
        if total > best_s or (total == best_s and e.player < best) then
            best, best_s = e.player, total
        end
    end
    for _, a in ipairs(own_armies()) do
        if a.strength > 0 then
            if hull_contains(tw, a.cx, a.cy) then
                sum = sum - a.strength
            else
                local px, py = hull_closest(tw, a.cx, a.cy)
                local dist = d2(px, py, a.cx, a.cy)
                if dist < OWN_ARMY_RANGE2 then
                    local v = dist * a.strength // OWN_ARMY_RANGE2
                    sum = sum - v
                    if fill then
                        local p = at(px, py)
                        p.own = p.own + v
                    end
                end
            end
        end
    end
    local threat = 0
    if sum > 0 then
        local total = own_strength()
        if total > 0 then
            threat = math.min(1000, sum * 1000 // total)
        else
            threat = 1000
        end
    end
    return threat, best, points
end

-- GetWorstThreats: the 3 strongest threat points, the largest net threat first
local function worst_threats(memory, tw)
    local _, _, points = town_threat(memory, tw, true)
    table.sort(points, function(a, b)
        if a.enemy ~= b.enemy then
            return a.enemy > b.enemy
        end
        return a.x * 10000 + a.y < b.x * 10000 + b.y
    end)
    local out = {}
    for k = 1, math.min(3, #points) do
        out[k] = points[k]
    end
    table.sort(out, function(a, b)
        if a.enemy - a.own ~= b.enemy - b.own then
            return a.enemy - a.own > b.enemy - b.own
        end
        return a.x * 10000 + a.y < b.x * 10000 + b.y
    end)
    return out, points
end

-- ---------------------------------------------------------------------------------------------------------------
-- The unit selector (EECPUnitSelector::Select)

-- GatherCandidates' classes by AI kind (GetScreenedTypesByLine with the currently-buildable screen, then
-- KeepBestClasses: per kind the classes of its latest epoch), of one domain (SplitByDomain: "land" or "sea").
-- DEVIATION: the air and space: NOT STARTED; the classes come from the view's costs (what the player can train)
local function candidate_classes(view, kind, domain)
    domain = domain or "land"
    local byKind = {}
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building then
            local k = cls(c.type)
            local t = U.class_tech(view, c.type)
            if k.domain == domain and (k.speed or 0) > 0 and (k.epoch or 0) <= view.epoch
                    and (k.attack_unit or k.defense_unit) and not k.is_transport
                    and t ~= nil and not t.obsolete and (k.ai_kind or 0) ~= 0 then
                -- FilterByStrategy: an attack group keeps the attack units, a defence group the defence units
                -- (the strategies of kind 4 and 5 keep both)
                if (kind == 0 and k.attack_unit) or (kind == 1 and k.defense_unit) or kind == 4 or kind == 5 then
                    byKind[k.ai_kind] = byKind[k.ai_kind] or {}
                    local l = byKind[k.ai_kind]
                    l[#l + 1] = c.type
                end
            end
        end
    end
    for _, ak in ipairs(ai.keys(byKind)) do
        local l = byKind[ak]
        local top = -1
        for _, name in ipairs(l) do
            top = math.max(top, cls(name).epoch or 0)
        end
        local keep = {}
        for _, name in ipairs(l) do
            if (cls(name).epoch or 0) == top then
                keep[#keep + 1] = name
            end
        end
        table.sort(keep)
        byKind[ak] = keep
    end
    return byKind
end

-- IsFavoriteLand
local function favorite_land_ok(name)
    local k = cls(name)
    local ak = k.ai_kind or 0
    return (k.speed or 0) > 0 and (k.attack or 0) > 0 and ak ~= KIND_HERO and not k.medic and not k.converts
        and ak ~= KIND_ANTI_TANK and ak ~= KIND_SPECIAL and ak ~= KIND_SIEGE and ak ~= KIND_ANTI_AIR
        and ak ~= KIND_ARTILLERY and not k.is_transport and not k.mine
end

-- GetUnitsAvailable: the military room, (1 - the citizen share - military / limit) x the limit, up
local function units_available(view, memory)
    local cap = view.population_cap
    if cap <= 0 then
        return 0
    end
    local military = U.military(view, memory, true)
    local v = (1000 - memory.t.behavior.citizen_percentage_k) * cap - military * 1000
    return v > 0 and (v + 999) // 1000 or v // 1000
end

-- GetUnitLimit
local function unit_limit(view, memory)
    return view.population_cap * (1000 - memory.t.behavior.citizen_percentage_k) // 1000
end

-- GetDomainShare(sea): the water's share of the map's tiles (x 0.85..1.15), cut by the own sea strength against the
-- target's (or by the own sea units' count when the target has none at sea), then pulled toward the target's sea
-- share of its strength by the air unit weight (as the original: GetAirUnitWeight); thousandths. 0 without a sea
local function sea_share(memory, f)
    local t = memory.t
    if (t.largest_sea or 0) == 0 then
        return 0
    end
    local water, space, land = t.water_tiles or 0, t.space_tiles or 0, t.land_tiles or 0
    local total = water + space + land
    if total <= 0 then
        return 0
    end
    local tdom = f.dom or {land = 0, sea = 0, air = 0, space = 0}
    local tsum = tdom.land + tdom.sea + tdom.air + tdom.space
    local own = own_domains()
    local share = tsum ~= 0 and tdom.sea * 1000 // tsum or 0
    local ratio
    if tdom.sea > 0 then
        ratio = own.sea ~= 0 and own.sea * 1000 // tdom.sea or 1000 - share
    else
        ratio = own.n_sea * 1000
    end
    local result = 0
    local fraction = water * 1000 // total
    if fraction > 0 then
        result = ai.random(850, 1150) * fraction // 1000
        if result > 1000 then
            result = fraction
        end
        if result > 0 then
            if ratio >= 1000 then
                result = result * 1000 // ratio
            elseif ratio > 0 then
                result = (1000 - ratio) * result // 1000
            else
                result = T.random_k() * result // 1000
            end
        end
    end
    if share > 0 and result > 0 then
        result = memory.t.behavior.air_unit_weight_k * (share - result) // 1000 + result
        result = math.max(0, math.min(1000, result))
    end
    return result
end

-- ComputeDomainShares' air share (from the Atomic Age - WW1): a weight, the target's town across water 0.1-0.3, more
-- anti-air units with the target than with the player 0.05-0.2, none of these 0.2-0.35 (planets 0-0.1); the share a
-- random 0.9-1.35 of it, pulled toward the target's air share of its strength by the naval unit weight (as the
-- original: mNavalUnitWeight); thousandths. DEVIATION: the town values' rule (a target town 1.5 x the player's
-- worth: 0.2-0.4) is not ported (the target's town values are not known)
local function air_share(view, memory, f, target)
    if view.epoch < 9 then
        return 0
    end
    local t = memory.t
    local weight = 0
    local tx, ty = R.town(view)
    local ex, ey
    for _, e in ipairs(view.enemies or {}) do
        if e.player == target and e.town_x ~= nil and e.town_x >= 0 then
            ex, ey = e.town_x, e.town_y
        end
    end
    if tx ~= nil and ex ~= nil and not ai.reach(tx, ty, ex, ey) and not t.space then
        weight = ai.random(100, 300)
    end
    local theirs, ours = 0, 0
    for _, ty2 in ipairs(f.types or {}) do
        if (cls(ty2).ai_kind or 0) == KIND_ANTI_AIR then
            theirs = theirs + 1
        end
    end
    for _, src in ipairs({own_military()}) do
        for _, u in ipairs(src) do
            if (cls(u.type).ai_kind or 0) == KIND_ANTI_AIR then
                ours = ours + 1
            end
        end
    end
    if ours < theirs then
        weight = weight + ai.random(50, 200)
    end
    if weight <= 0 then
        weight = t.planets and ai.random(0, 100) or ai.random(200, 350)
    end
    local result = ai.random(900, 1350) * weight // 1000
    if result > 1000 then
        result = 950
    elseif result < 1 then
        result = 0
    end
    local tdom = f.dom or {land = 0, sea = 0, air = 0, space = 0}
    local tsum = tdom.land + tdom.sea + tdom.air + tdom.space
    if tsum ~= 0 then
        local share = tdom.air * 1000 // tsum
        if share > 0 and result > 0 then
            result = (share - result) * (t.behavior.naval_unit_weight_k or 0) // 1000 + result
            result = math.max(0, math.min(1000, result))
        end
    end
    return result
end

-- the air counterpart of IsFavoriteLand (EECPUSFavoriteUnitsScreen::IsFavoriteAir). DEVIATION: the favourite air type
-- is a random trainable air class of the selection (ChooseFavoriteAirType's screen is not readable)

-- the sea counterpart of IsFavoriteLand (EECPUSFavoriteUnitsScreen::IsFavoriteSea)
local function favorite_sea_ok(name)
    local k = cls(name)
    local ak = k.ai_kind or 0
    return (k.speed or 0) > 0 and (k.attack or 0) > 0 and ak ~= KIND_HERO and not k.medic and not k.converts
        and ak ~= KIND_SPECIAL and not k.is_transport and not k.mine
end
M.favorite_sea_ok = favorite_sea_ok

-- the space counterpart (EECPUSFavoriteUnitsScreen::IsFavoriteSpace): as the sea's, and no anti-air, artillery,
-- siege, anti-submarine helicopter or Mech Zeus kind
function M.favorite_space_ok(name)
    local ak = cls(name).ai_kind or 0
    return favorite_sea_ok(name) and ak ~= KIND_ANTI_AIR and ak ~= KIND_ARTILLERY and ak ~= KIND_SIEGE
        and ak ~= KIND_ANTI_SUB_HELICOPTER and ak ~= KIND_MECH_ZEUS
end

-- AllocateCounterUnits for one domain: the classes whose family counters the target's types (weights 3/2/1), up to
-- `maximum` classes, each in the choices its weight's times; ceil(n x the RPS factor) picks. -> the slots, n left
local function counter_slots(memory, byKind, weights, list, n, maximum, siege_limit)
    local b = memory.t.behavior
    local pool = {}
    for _, ak in ipairs(ai.keys(byKind)) do
        for _, name in ipairs(byKind[ak]) do
            pool[#pool + 1] = name
        end
    end
    local limited = 0
    shuffle(pool)
    local choices, specials, count = {}, {}, 0
    for i = #pool, 1, -1 do
        local name = pool[i]
        local k = cls(name)
        if k.ai_kind == KIND_SPECIAL then
            specials[#specials + 1] = name
        elseif count < maximum and weights[k.ai_kind] ~= nil then
            if k.medic then
                choices[#choices + 1] = name
            else
                for _ = 1, weights[k.ai_kind] do
                    choices[#choices + 1] = name
                end
                count = count + 1
            end
            if siege_limit ~= nil and (k.ai_kind == KIND_SIEGE or k.ai_kind == KIND_ARTILLERY) then
                limited = limited + 1
                if limited >= siege_limit then
                    weights[k.ai_kind] = nil
                end
            end
        end
    end
    local slots = {}
    if #choices > 0 then
        local factor = memory.t.island and b.rps_selection_islands_k or b.rps_selection_non_islands_k
        local picks = (n * factor + 999) // 1000
        for _ = 1, picks do
            local name
            if #specials > 0 and T.random_k() < b.specialized_unit_chance_k then
                name = specials[ai.random(1, #specials)]
            else
                -- TakeUnitClass(the first choice): a free unit of it when there is one
                local first = choices[1]
                for _, u in ipairs(list) do
                    if u.type == first then
                        name = first
                        break
                    end
                end
                if name == nil then
                    name = choices[ai.random(1, #choices)]
                end
            end
            slots[#slots + 1] = name
            n = math.max(n - 1, 0)
        end
    end
    return slots, n
end

-- the favourite of a domain (AllocateFavoriteUnits): the player's favourite type when available, else a random
-- favourite candidate of the domain's classes; nil for none
local function favorite_of(view, fav, domain, byKind, ok_fn)
    if fav ~= nil then
        local t = U.class_tech(view, fav)
        if cls(fav).domain == domain and t ~= nil and t.available then
            return fav
        end
    end
    local list2 = {}
    for _, ak in ipairs(ai.keys(byKind)) do
        for _, name in ipairs(byKind[ak]) do
            local t = U.class_tech(view, name)
            if t ~= nil and t.available and ok_fn(name) then
                list2[#list2 + 1] = name
            end
        end
    end
    return #list2 > 0 and list2[ai.random(1, #list2)] or nil
end

-- AllocateSpecialUnits' priests and prophets: by the personality's chance (priest_chance, prophet_chance) when the
-- strategy's kind takes the class (an attack group an attack unit, a defence group a defence unit), 1 .. the most per
-- group (and the land count - 1), under its share of the population limit (pop_percentage, the own and asked for
-- counted): a free one of the class taken -> the wanted count asked for; else the held candidates of the class, else
-- the class asked for when a candidate. As the original, the free-one path leaves the land count as it is.
-- The paratroopers: NOT STARTED (the carriers: in select_units)
local function special_slots(view, memory, kind, numLand, numUnits, list, cands, held, byKind)
    local b = memory.t.behavior
    local slots = {}
    local cap = view.population_cap
    local specs = {{name = "priest", chance = b.priest_chance_k, max = b.priest_max_per_group,
                    pop = b.priest_pop_percentage_k, need_units = false},
                   {name = "prophet", chance = b.prophet_chance_k, max = b.prophet_max_per_group,
                    pop = b.prophet_pop_percentage_k, need_units = true}}
    for _, sp in ipairs(specs) do
        local k = cls(sp.name)
        local takes = kind == 4 or kind == 5 or (k.attack_unit and kind == 0) or (k.defense_unit and kind == 1)
        if numLand > 0 and (not sp.need_units or numUnits > 1) and takes and T.random_k() <= (sp.chance or 0) then
            local wanted = ai.random(1, math.max(math.min(sp.max or 0, numLand - 1), 1))
            local have = #ai.units{type = sp.name}
            for _, g in ipairs(G.all(army(memory))) do
                for _, sl in ipairs(g.slots or {}) do
                    if sl.c == sp.name then
                        have = have + 1
                    end
                end
            end
            local limit = ((sp.pop or 0) * cap + 999) // 1000
            if have < limit then
                wanted = math.min(wanted, limit - have)
                if wanted > 0 then
                    local free = false
                    for _, u in ipairs(list) do
                        if u.type == sp.name then
                            free = true
                            break
                        end
                    end
                    if not free then
                        for _, c in ipairs(cands) do
                            if wanted <= 0 then
                                break
                            end
                            if c.type == sp.name and not c.taken then
                                c.taken = true
                                held[#held + 1] = c
                                numLand = math.max(numLand - 1, 0)
                                wanted = wanted - 1
                            end
                        end
                        if wanted > 0 then
                            for _, ak in ipairs(ai.keys(byKind)) do
                                for _, name in ipairs(byKind[ak]) do
                                    if name == sp.name then
                                        free = true
                                    end
                                end
                            end
                            if free then
                                numLand = math.max(numLand - wanted, 0)
                            end
                        end
                    end
                    if free then
                        for _ = 1, wanted do
                            slots[#slots + 1] = sp.name
                        end
                    end
                end
            end
        end
    end
    -- the heroes (AI kind 0x1b): by hero_chance with more than one unit in the group, 1 .. hero_max_per_group, under
    -- hero_pop_percentage of the limit: a free one taken, else the held heroes, else a random hero class of the
    -- candidates. DEVIATION: the game's hero limit (GetHeroLimit) is not read
    local heroes = byKind[KIND_HERO]
    if numLand > 0 and numUnits > 1 and T.random_k() <= (b.hero_chance_k or 0) and heroes ~= nil and #heroes > 0 then
        local wanted = ai.random(1, math.max(math.min(b.hero_max_per_group or 0, numLand - 1), 1))
        local have = 0
        for _, u in ipairs(own_military()) do
            if (cls(u.type).ai_kind or 0) == KIND_HERO then
                have = have + 1
            end
        end
        for _, g in ipairs(G.all(army(memory))) do
            for _, sl in ipairs(g.slots or {}) do
                if (cls(sl.c).ai_kind or 0) == KIND_HERO then
                    have = have + 1
                end
            end
        end
        local limit = ((b.hero_pop_percentage_k or 0) * cap + 999) // 1000
        if have < limit then
            wanted = math.min(wanted, limit - have)
            local taken = nil
            for _, u in ipairs(list) do
                if (cls(u.type).ai_kind or 0) == KIND_HERO then
                    taken = u.type
                    break
                end
            end
            if taken ~= nil then
                slots[#slots + 1] = taken
                wanted = wanted - 1
            end
            for _, c in ipairs(cands) do
                if wanted <= 0 then
                    break
                end
                if (cls(c.type).ai_kind or 0) == KIND_HERO and not c.taken then
                    c.taken = true
                    held[#held + 1] = c
                    numLand = math.max(numLand - 1, 0)
                    wanted = wanted - 1
                end
            end
            if wanted > 0 then
                local name = heroes[ai.random(1, #heroes)]
                for _ = 1, wanted do
                    numLand = math.max(numLand - 1, 0)
                    slots[#slots + 1] = name
                end
            end
        end
    end
    return slots, numLand
end

-- Select: the classes of a new group's slots and the held units it takes, land and sea. kind 0 attack, 1 defence.
-- The candidates are the free soldiers and warships (and, for an attack, the defence groups' soldiers:
-- GatherDefendingUnits). ComputeDomainShares: the sea share (GetDomainShare); the air share (from WW1) and space:
-- NOT STARTED
local function select_units(view, memory, kind, target, naval, domain)
    local b = memory.t.behavior
    local mem = army(memory)
    local pool, list = free_pool()
    local cands = {}
    for _, u in ipairs(list) do
        cands[#cands + 1] = {id = u.id, type = u.type}
    end
    if kind == 0 then
        for _, g in ipairs(G.all(mem)) do
            if g.mission == 1 and g.phase == "task" and g.state ~= "fighting" then
                for _, id in ipairs(g.units) do
                    local u = R.unit(id)
                    if u ~= nil and u.inside == nil and (u.attack or 0) > 0 then
                        cands[#cands + 1] = {id = id, type = u.type, from = g.id}
                    end
                end
            end
        end
    end
    local f = file_of(memory, target)
    local strength = strength_of(view, memory, f)
    if naval ~= nil then
        -- (CheckNaval's strategy: the count set, the sea's share 100, a counter requirement and the favourites)
        return M.select_naval(view, memory, kind, f, strength, list, naval, domain)
    end
    -- ComputeGroupSize
    local share
    if kind == 1 then
        share = T.random(b.defense_group_size_min, b.defense_group_size_max)
    else
        share = T.random(b.attack_group_size_min, b.attack_group_size_max)
    end
    local count = share * strength // 100
    if count > 0 then
        if kind == 0 then
            count = count * T.by_difficulty(memory, b.attack_group_modifier_easy, b.attack_group_modifier_medium,
                b.attack_group_modifier_hard) // 100
        end
    else
        count = 1
    end
    local room = units_available(view, memory)   -- (+ ChooseUnitsToKill: NOT STARTED, 0)
    local held_n, favorites, counters = 0, true, true
    if count > room then
        if room > 0 then
            held_n = count - room
            local short = #cands - held_n
            if short < 0 then
                count = count + short
            end
            held_n = math.min(held_n, #cands)
        else
            count = math.min(count, #cands)
            favorites, counters = false, false
            held_n = count
        end
    end
    if count <= 0 then
        return nil
    end
    -- ComputeDomainShares and Calculate: the air's count (from WW1), the sea's on what is left (the cap x a share
    -- must make a unit), the rest land
    local numSea, numAir = 0, 0
    local air_k = air_share(view, memory, f, target)
    local sea_k = math.min(sea_share(memory, f), 1000 - air_k)
    local cap = view.population_cap
    local land_k = 1000 - sea_k - air_k
    if land_k < 1 then
        land_k = 0
    end
    local sum = 0
    if air_k > 0 and cap * air_k // 1000 > 0 then
        sum = sum + air_k
    else
        air_k = 0
    end
    if sea_k > 0 and cap * sea_k // 1000 > 0 then
        sum = sum + sea_k
    else
        sea_k = 0
    end
    if land_k > 0 and cap * land_k // 1000 > 0 then
        sum = sum + land_k
    end
    if sum > 0 and sea_k > 0 then
        numSea = sea_k * count // sum
    end
    if sum > 0 and air_k > 0 then
        numAir = air_k * count // sum
    end
    local byKind = candidate_classes(view, kind, "land")
    local seaKind = candidate_classes(view, kind, "sea")
    local airKind = candidate_classes(view, kind, "air")
    -- RedistributeDomainCounts: a domain with no class gives its count to land
    if numSea > 0 and #ai.keys(seaKind) == 0 then
        numSea = 0
    end
    if numAir > 0 and #ai.keys(airKind) == 0 then
        numAir = 0
    end
    local numLand = count - numSea - numAir
    local held, slots = {}, {}
    -- AllocateHeldUnits: the most common classes first (up to 9 classes), each domain within its count
    if held_n > 0 then
        local counts = {}
        for _, c in ipairs(cands) do
            local k = cls(c.type)
            if (k.ai_kind or 0) ~= KIND_HERO then
                counts[c.type] = (counts[c.type] or 0) + 1
            end
        end
        local wanted = held_n
        for _ = 0, 8 do
            if wanted <= 0 then
                break
            end
            local best, bn = nil, 0
            for _, t in ipairs(ai.keys(counts)) do
                if counts[t] > bn then
                    best, bn = t, counts[t]
                end
            end
            if best == nil then
                break
            end
            local dom = domain_of(cls(best))
            local sea = dom == "sea"
            local airc = dom == "air"
            local take = math.min(bn, wanted, sea and numSea or airc and numAir or numLand)
            for _, c in ipairs(cands) do
                if take <= 0 then
                    break
                end
                if c.type == best and not c.taken then
                    c.taken = true
                    held[#held + 1] = c
                    take = take - 1
                    wanted = wanted - 1
                    if sea then
                        numSea = numSea - 1
                    elseif airc then
                        numAir = numAir - 1
                    else
                        numLand = numLand - 1
                    end
                end
            end
            counts[best] = nil
        end
    end
    -- AllocateSpecialUnits: the priests and prophets
    do
        local got
        got, numLand = special_slots(view, memory, kind, numLand, count, list, cands, held, byKind)
        for _, name in ipairs(got) do
            slots[#slots + 1] = name
        end
    end
    -- AllocateSpecialUnits' aircraft carriers: with a sea count and more than one unit, by the carrier chance, 1 up
    -- to the most per group (less one sea unit), while the carriers (own and asked for) stay under their share of the
    -- military room; a class of the carriers' kind among the sea candidates (as the C++: none, no carriers)
    do
        local b = memory.t.behavior
        local ck = cls(CARRIER)
        local takes = kind == 4 or kind == 5 or (ck.attack_unit and kind == 0) or (ck.defense_unit and kind == 1)
        if numSea > 0 and count > 1 and ck.ai_kind ~= nil and takes and T.random_k() <= (b.carrier_chance_k or 0) then
            local wanted = ai.random(1, math.max(math.min(b.carrier_max_per_group or 0, numSea - 1), 1))
            local have = 0
            for _, u in ipairs(own_ships()) do
                if (cls(u.type).ai_kind or 0) == KIND_CARRIER then
                    have = have + 1
                end
            end
            for _, g in ipairs(G.all(army(memory))) do
                for _, sl in ipairs(g.slots or {}) do
                    if (cls(sl.c or sl).ai_kind or 0) == KIND_CARRIER then
                        have = have + 1
                    end
                end
            end
            local limit = ((b.carrier_pop_percentage_k or 0) * unit_limit(view, memory) + 999) // 1000
            if have < limit then
                wanted = math.min(wanted, limit - have)
                local names = {}
                for _, ak in ipairs(ai.keys(seaKind)) do
                    for _, name in ipairs(seaKind[ak]) do
                        if (cls(name).ai_kind or 0) == KIND_CARRIER then
                            names[#names + 1] = name
                        end
                    end
                end
                table.sort(names)
                if #names > 0 then
                    local pick = names[ai.random(1, #names)]
                    for _ = 1, wanted do
                        if numSea <= 0 then
                            break
                        end
                        slots[#slots + 1] = pick
                        numSea = numSea - 1
                    end
                end
            end
        end
    end
    -- AllocateCounterUnits: the families that counter the target's unit types, weighted 3/2/1 (WeighCounters: the
    -- families with a candidate class), the sea (3-5 classes) before the land (2-6, the siege share)
    if counters and (numLand > 0 or numSea > 0) then
        local all = {}
        for _, ak in ipairs(ai.keys(byKind)) do
            all[ak] = true
        end
        for _, ak in ipairs(ai.keys(seaKind)) do
            all[ak] = true
        end
        for _, ak in ipairs(ai.keys(airKind)) do
            all[ak] = true
        end
        local weights = {}
        for _, t in ipairs(f.types or {}) do
            local k = cls(t)
            local ws = {3, 2, 1}
            for i, ck in ipairs(k.counters or {}) do
                if ck ~= 0 and all[ck] then
                    weights[ck] = (weights[ck] or 0) + ws[i]
                end
            end
        end
        if #ai.keys(weights) > 0 then
            if numAir > 0 then
                local got
                got, numAir = counter_slots(memory, airKind, weights, list, numAir, ai.random(3, 5), nil)
                for _, name in ipairs(got) do
                    slots[#slots + 1] = name
                end
            end
            if numSea > 0 then
                local got
                got, numSea = counter_slots(memory, seaKind, weights, list, numSea, ai.random(3, 5), nil)
                for _, name in ipairs(got) do
                    slots[#slots + 1] = name
                end
            end
            if numLand > 0 then
                local got
                got, numLand = counter_slots(memory, byKind, weights, list, numLand, ai.random(2, 6),
                    (b.siege_weapon_percentage_k * numLand + 999) // 1000)
                for _, name in ipairs(got) do
                    slots[#slots + 1] = name
                end
            end
        end
    end
    -- AllocateFavoriteUnits: the air count a favourite air type (none: the count goes to land), the sea count the
    -- favourite sea type (none: to land), then the land count the favourite land type
    if favorites and numAir > 0 then
        local names = {}
        for _, ak in ipairs(ai.keys(airKind)) do
            for _, name in ipairs(airKind[ak]) do
                names[#names + 1] = name
            end
        end
        table.sort(names)
        if #names > 0 then
            local fav = names[ai.random(1, #names)]
            for _ = 1, numAir do
                slots[#slots + 1] = fav
            end
        else
            numLand = numLand + numAir
        end
        numAir = 0
    end
    if numAir > 0 then
        numLand = numLand + numAir
        numAir = 0
    end
    if favorites and numSea > 0 then
        local fav = favorite_of(view, memory.cp.favorite_sea, "sea", seaKind, favorite_sea_ok)
        if fav ~= nil then
            for _ = 1, numSea do
                slots[#slots + 1] = fav
            end
        else
            numLand = numLand + numSea
        end
        numSea = 0
    end
    if favorites and numLand > 0 then
        local fav = favorite_of(view, memory.cp.favorite_land, "land", byKind, favorite_land_ok)
        if fav ~= nil then
            for _ = 1, numLand do
                slots[#slots + 1] = fav
            end
        end
    end
    return {count = count, held = held, slots = slots, strength = strength, sea_k = sea_k, air_k = air_k}
end

-- CheckNaval's selection: `count` units, all at sea (no sea class: they go to land, as AllocateFavoriteUnits moves
-- the count), the counter requirement first, then the favourites. CheckSpace's: the same in space (domain "space",
-- the favourite space type)
function M.select_naval(view, memory, kind, f, strength, list, count, domain)
    local b = memory.t.behavior
    domain = domain or "sea"
    local byKind = candidate_classes(view, kind, "land")
    local seaKind = candidate_classes(view, kind, domain)
    local numSea, numLand = count, 0
    if #ai.keys(seaKind) == 0 then
        numSea, numLand = 0, count
    end
    local slots = {}
    local all = {}
    for _, ak in ipairs(ai.keys(byKind)) do
        all[ak] = true
    end
    for _, ak in ipairs(ai.keys(seaKind)) do
        all[ak] = true
    end
    local weights = {}
    for _, t in ipairs(f.types or {}) do
        local k = cls(t)
        local ws = {3, 2, 1}
        for i, ck in ipairs(k.counters or {}) do
            if ck ~= 0 and all[ck] then
                weights[ck] = (weights[ck] or 0) + ws[i]
            end
        end
    end
    if #ai.keys(weights) > 0 then
        local got
        if numSea > 0 then
            got, numSea = counter_slots(memory, seaKind, weights, list, numSea, ai.random(3, 5), nil)
            for _, name in ipairs(got) do
                slots[#slots + 1] = name
            end
        end
        if numLand > 0 then
            got, numLand = counter_slots(memory, byKind, weights, list, numLand, ai.random(2, 6),
                (b.siege_weapon_percentage_k * numLand + 999) // 1000)
            for _, name in ipairs(got) do
                slots[#slots + 1] = name
            end
        end
    end
    if numSea > 0 then
        local fav
        if domain == "space" then
            fav = favorite_of(view, memory.cp.favorite_space, "space", seaKind, M.favorite_space_ok)
        else
            fav = favorite_of(view, memory.cp.favorite_sea, "sea", seaKind, favorite_sea_ok)
        end
        if fav ~= nil then
            for _ = 1, numSea do
                slots[#slots + 1] = fav
            end
        else
            numLand = numLand + numSea
        end
    end
    if numLand > 0 then
        local fav = favorite_of(view, memory.cp.favorite_land, "land", byKind, favorite_land_ok)
        if fav ~= nil then
            for _ = 1, numLand do
                slots[#slots + 1] = fav
            end
        end
    end
    return {count = count, held = {}, slots = slots, strength = strength, sea_k = domain == "sea" and 1000 or 0}
end

-- ---------------------------------------------------------------------------------------------------------------
-- The sub-groups

local function set_place(g, task, x, y, radius, target)
    g.task = task
    g.x, g.y = x, y
    g.radius = radius or 0
    g.target = target or 0
    g.way_t = 0
    g.aims = {}
    if g.state ~= "forming" then
        g.state = "moving"
        g.state_t = ai.time()
    end
end

local function rally_point(view)
    return view.town_x, view.town_y
end

-- CreateGroup (MakePlan, PlaceSubGroups): a sub-group of the selection, its held units taken, its slots requested
-- through Personnel. Returns the group, or nil
local function create_group(view, memory, kind, mission, target, why, naval, domain)
    local mem = army(memory)
    local sel = select_units(view, memory, kind, target, naval, domain)
    if sel == nil or (#sel.held == 0 and #sel.slots == 0) then
        print(string.format("classic_lua army.%s kind=%d target=%d none", why, kind, target or 0))
        return nil
    end
    local rx, ry = rally_point(view)
    if rx == nil or rx < 0 then
        return nil
    end
    -- MakeSubGroups: one sub-group per kind (GetSubGroupKind): land (with the helicopters, which fly over: a kind
    -- of land sub-group), sea (the warships), air (the planes: EECPASGAir, a manual group run by air_update) and
    -- space (EECPASGSpaceFighter: the air sub-group's life, every unit flying to its target as a fighter does;
    -- DEVIATION: the planet spaceships too, which the C++ gives a sub-group of their own)
    local parts = {land = {slots = {}, ids = {}}, sea = {slots = {}, ids = {}}, air = {slots = {}, ids = {}},
                   space = {slots = {}, ids = {}}}
    local function part_of(c)
        local d = domain_of(c)
        if d == "space" and c.is_transport then
            return "land"
        end
        return d
    end
    for _, name in ipairs(sel.slots) do
        local part = parts[part_of(cls(name))]
        part.slots[#part.slots + 1] = {c = name, asked = false}
    end
    for _, c in ipairs(sel.held) do
        local part = parts[part_of(cls(c.type))]
        part.ids[#part.ids + 1] = c.id
    end
    local first = nil
    for _, d in ipairs({"land", "sea", "air", "space"}) do
        local part = parts[d]
        if #part.slots > 0 or #part.ids > 0 then
            local g = G.create(mem, {task = "defend", role = "army", x = rx, y = ry, radius = GUARD_RADIUS, target = 0,
                                     manual = d == "air" or d == "space"})
            g.kind, g.mission, g.enemy = kind, mission, target or 0
            g.sea = d == "sea"
            if d == "air" or d == "space" then
                g.air = true
                g.space = d == "space"
                g.state = "air"     -- (no forming walk: the planes take off from their bases)
            end
            g.phase, g.phase_t = "wait", view.time
            g.deadline = view.time + REQUEST_MS
            g.slots = part.slots
            if #part.ids > 0 then
                G.add(mem, g, part.ids)
            end
            local nair, ncarrier = 0, 0
            for _, sl in ipairs(g.slots) do
                local k = cls(sl.c or sl)
                if domain_of(k) == "air" then
                    nair = nair + 1
                end
                if (k.ai_kind or 0) == KIND_CARRIER then
                    ncarrier = ncarrier + 1
                end
            end
            print(string.format("classic_lua army.%s id=%d kind=%d mission=%d target=%d size=%d held=%d slots=%d str=%d sea=%d air=%d airslots=%d carriers=%d%s",
                why, g.id, kind, mission, target or 0, sel.count, #part.ids, #g.slots, sel.strength, sel.sea_k,
                sel.air_k or 0, nair, ncarrier, g.sea and " navy" or (g.space and " space" or (g.air and " air" or ""))))
            first = first or g
        end
    end
    return first
end

-- Personnel's FillRequest: each open slot takes a free unit of its class, else one is ordered (once); the
-- deadline drops the open slots (Cancel)
local function fill_requests(view, memory)
    local mem = army(memory)
    local pool, list = free_pool()
    local used = {}
    for _, g in ipairs(G.all(mem)) do
        if g.slots ~= nil and #g.slots > 0 then
            if view.time >= (g.deadline or 0) then
                -- Cancel: the open slots go, and one production order each with them
                for _, sl in ipairs(g.slots) do
                    if sl.asked then
                        U.cancel_request(memory, sl.c, 1)
                    end
                end
                g.slots = {}
            else
                local ids, keep = {}, {}
                for _, s in ipairs(g.slots) do
                    local got = nil
                    for _, u in ipairs(list) do
                        if not used[u.id] and u.type == s.c then
                            got = u.id
                            break
                        end
                    end
                    if got ~= nil then
                        used[got] = true
                        ids[#ids + 1] = got
                    else
                        if not s.asked then
                            U.request(view, memory, s.c, 1)
                            s.asked = true
                            s.asked_t = view.time
                        end
                        keep[#keep + 1] = s
                    end
                end
                g.slots = keep
                if #ids > 0 then
                    G.add(mem, g, ids)
                end
            end
        end
    end
end

-- the population the army groups take (their units, their open slots): one ai.get over every unit (a record per
-- unit through ai.unit blew the step's budget with a large army)
local function groups_pop(memory)
    do
        local ids, n = {}, 0
        for _, g in ipairs(G.all(army(memory))) do
            if g.role == "army" then
                for _, id in ipairs(g.units) do
                    ids[#ids + 1] = id
                end
                for _, sl in ipairs(g.slots or {}) do
                    n = n + (cls(sl.c).pop or 1)
                end
            end
        end
        if #ids > 0 then
            local cols = ai.get(ids, {"type"})
            for k = 1, #ids do
                local t = cols.type[k]
                n = n + (t ~= nil and (cls(t).pop or 1) or 1)
            end
        end
        return n
    end
end

-- GetUnitAllowance: (defensive_unit_cap - the groups' population / the unit limit) x the limit
local function unit_allowance(view, memory)
    local total = unit_limit(view, memory)
    if total <= 0 then
        return 0
    end
    local used = groups_pop(memory)
    local cap = memory.t.behavior.defensive_unit_cap_k
    local frac = used * 1000 // total
    if frac >= cap then
        return 0
    end
    return (cap - frac) * total // 1000
end

-- the town nearest a tile (FindThreatenedTown's fall-back, GetNearestTown)
local function nearest_town(x, y)
    local best, bd = nil, nil
    for _, tw in ipairs(towns()) do
        local d = d2(tw.cx, tw.cy, x, y)
        if bd == nil or d < bd then
            best, bd = tw, d
        end
    end
    return best
end

-- FindThreatenedTown: the most threatened own town (a package's threat for a town that has one)
local function threatened_town(memory)
    local a = army(memory)
    local best, best_t = nil, 0
    local list = towns()
    for k = #list, 1, -1 do
        local tw = list[k]
        local pk = a.packages[tw.id]
        local t
        if pk ~= nil then
            t = pk.threat
        else
            t = town_threat(memory, tw, false)
        end
        if best == nil or best_t < t then
            best, best_t = tw, t
        end
    end
    return best, best_t
end

-- the target regions of a player (EECPIntelligencePlayerFile::GetTargets): one remembered land building per
-- 9-tile region, by the player's influence within 2 regions around, the strongest first
local function target_regions(memory, player)
    local f = file_of(memory, player)
    local regions, infl = {}, {}
    for _, bld in ipairs(f.buildings or {}) do
        local key = (bld.x // REGION) * 1000 + bld.y // REGION
        if regions[key] == nil or bld.id < regions[key].id then
            regions[key] = bld
        end
        infl[key] = (infl[key] or 0) + bld.influence
    end
    local out = {}
    for _, key in ipairs(ai.keys(regions)) do
        local bld = regions[key]
        local rx, ry = bld.x // REGION, bld.y // REGION
        local s = 0
        for dx = -2, 2 do
            for dy = -2, 2 do
                s = s + (infl[(rx + dx) * 1000 + ry + dy] or 0)
            end
        end
        out[#out + 1] = {x = bld.x, y = bld.y, score = s, id = bld.id}
    end
    table.sort(out, function(p, q)
        if p.score ~= q.score then
            return p.score > q.score
        end
        return p.id < q.id
    end)
    return out
end

-- a random point on the group's land (EEAIContinent::GetRandomPoint): eight tries
local function continent_point(view, x, y)
    local here = ai.tile(x, y)
    local cont = here ~= nil and here.continent or nil
    local m = ai.map()
    for _ = 1, 8 do
        local px, py = ai.random(0, m.width - 1), ai.random(0, m.height - 1)
        local t = ai.tile(px, py)
        if t ~= nil and t.land and (cont == nil or t.continent == cont) then
            return px, py
        end
    end
    return nil
end

local function add_to_package(memory, town_id, g)
    local pk = army(memory).packages[town_id]
    if pk ~= nil then
        for _, id in ipairs(pk.groups) do
            if id == g.id then
                return
            end
        end
        pk.groups[#pk.groups + 1] = g.id
    end
end

-- AssignSubGroupTask (the defence ladder): the chosen defend tasks (scenario only: none); a forest within 35 tiles
-- (80%, infantry); a hill within 35 tiles (35%: ai.hill_point), a choke point of the land (guard_chokepoint_percentage:
-- ai.choke_point; the group has no continent of its own, so any of them); a threatened town (guard_town_percentage:
-- mission 3, into its package); a random point on the group's land. NOT STARTED: the adjacent continents' step
local function defence_task(view, memory, g)
    local b = memory.t.behavior
    local x, y = g.cx or g.x, g.cy or g.y
    if x == nil or x < 0 then
        x, y = rally_point(view)
    end
    -- the forest (UpdateCommonMoveFlags: every unit of the group crosses forest, the records' crosses_forest)
    local infantry = true
    if #g.units > 0 then
        local cols = ai.get(g.units, {"crosses_forest"})
        for k = 1, #g.units do
            if not cols.crosses_forest[k] then
                infantry = false
            end
        end
    end
    if infantry and T.random_k() < 800 then
        -- the forests (the continent manager's) whose hull comes within 35 tiles, one at random, a random point of it
        -- (ai.forest_point); without the query a tree within 35 tiles
        if ai.forest_point ~= nil then
            local fx, fy = ai.forest_point(x, y, FOREST_RANGE, ai.random(0, 1 << 30), ai.random(1, 0x7fffffff))
            if fx ~= nil then
                g.mission = 1
                set_place(g, "defend", fx, fy, GUARD_RADIUS, 0)
                return "forest"
            end
        else
            local trees = ai.resources{kind = "wood", near = {x, y, FOREST_RANGE}, limit = 40}
            if #trees > 0 then
                local r = R.unit(trees[ai.random(1, #trees)])
                if r ~= nil then
                    g.mission = 1
                    set_place(g, "defend", r.x, r.y, GUARD_RADIUS, 0)
                    return "forest"
                end
            end
        end
    end
    -- a hill whose hull comes within 35 tiles (35%), else a choke point of the land (guard_chokepoint_percentage)
    if ai.hill_point ~= nil and T.random_k() < 350 then
        local hx, hy = ai.hill_point(x, y, FOREST_RANGE, ai.random(0, 1 << 30), ai.random(1, 0x7fffffff))
        if hx ~= nil then
            g.mission = 1
            set_place(g, "defend", hx, hy, GUARD_RADIUS, 0)
            return "hill"
        end
    end
    if ai.choke_point ~= nil and T.random_k() < (b.guard_chokepoint_percentage_k or 0) then
        local cx, cy = ai.choke_point(x, y, 0, ai.random(0, 1 << 30), ai.random(1, 0x7fffffff))
        if cx ~= nil then
            g.mission = 1
            set_place(g, "defend", cx, cy, GUARD_RADIUS, 0)
            return "choke"
        end
    end
    if T.random_k() < b.guard_town_percentage_k then
        local tw = threatened_town(memory)
        if tw ~= nil then
            add_to_package(memory, tw.id, g)
            g.mission = 3
            set_place(g, "defend", ai.random(tw.l, tw.r), ai.random(tw.t, tw.b), GUARD_RADIUS, 0)
            return "town"
        end
    end
    local px, py = continent_point(view, x, y)
    if px ~= nil then
        g.mission = 1
        set_place(g, "defend", px, py, GUARD_RADIUS, 0)
        return "land"
    end
    return nil
end

-- a random point of a water body (EEAIContinent::GetRandomPoint, ai.area_point with the script's draw)
local function sea_point(body)
    return ai.area_point(body, ai.random(1, 0x7fffffff))
end

-- AssignSeaTask: FindSeaTarget finds nothing (the original's); a quarter of the time a known unit of the target
-- player on the sub-group's water (its main continent); else a random point of that water (the defend tasks of
-- ChooseTask: scenario only, none); else the water by the town. DEVIATION (fog): the known units are those
-- remembered (ai.memory)
local function assign_sea_task(view, memory, g)
    local x, y = g.cx or g.x, g.cy or g.y
    -- (GetMainContinent: the water body under the sub-group, else the largest)
    local here = (x ~= nil and x >= 0) and ai.area(x, y) or nil
    local body = (here ~= nil and here.category == 2) and here.id or 0
    if body == 0 then
        body = memory.t.largest_sea or 0
    end
    local why = nil
    if body ~= 0 and T.random_k() < 250 then
        local list = {}
        for _, m in ipairs(ai.memory{player = g.enemy}) do
            if not m.building then
                local t = ai.area(m.x, m.y)
                if t ~= nil and t.id == body then
                    list[#list + 1] = m
                end
            end
        end
        if #list > 0 then
            local m = list[ai.random(1, #list)]
            set_place(g, "attack", m.x, m.y, 0, g.enemy)
            why = "unit"
        end
    end
    if why == nil and body ~= 0 then
        local px, py = sea_point(body)
        if px ~= nil then
            set_place(g, "defend", px, py, GUARD_RADIUS, 0)
            why = "sea"
        end
    end
    if why == nil then
        local rx, ry = rally_point(view)
        local sites = rx ~= nil and ai.water_sites(rx, ry, {max = 1}) or {}
        if #sites > 0 then
            rx, ry = sites[1][1], sites[1][2]
        end
        set_place(g, "defend", rx, ry, GUARD_RADIUS, 0)
        why = "return"
    end
    g.task_t = view.time
    g.task_check = view.time + ai.random(10, 120) * 1000
    print(string.format("classic_lua army.task id=%d mission=%d %s at=%d,%d units=%d navy", g.id, g.mission, why, g.x,
        g.y, #g.units))
end

-- AssignLandTask: an attack (mission 0 or 2) goes for the target's territory (the weakest region first); else the
-- defence ladder; else back to the rally point. A sea sub-group: AssignSeaTask
local function assign_task(view, memory, g)
    if g.sea then
        return assign_sea_task(view, memory, g)
    end
    local why = nil
    local home = g.mission == 3 and g.task_town ~= nil and town_by_id(g.task_town) or nil
    if home ~= nil then
        -- (CounterAttack's task: the town, in combat mode)
        set_place(g, "defend", ai.random(home.l, home.r), ai.random(home.t, home.b), GUARD_RADIUS, 0)
        why = "aid"
    elseif g.mission == 2 or g.mission == 0 then
        local regions = target_regions(memory, g.enemy)
        local pick = regions[#regions]
        if pick ~= nil and g.last_x == pick.x and g.last_y == pick.y then
            -- (IsTerritoryTaskValid: a region reached and still known is done; the next one)
            pick = regions[#regions - 1]
        end
        if pick ~= nil then
            g.mission = 2
            g.last_x, g.last_y = pick.x, pick.y
            set_place(g, "attack", pick.x, pick.y, 0, g.enemy)
            why = "territory"
        else
            g.mission = 0
        end
    end
    if why == nil then
        why = defence_task(view, memory, g)
    end
    if why == nil then
        local rx, ry = rally_point(view)
        set_place(g, "defend", rx, ry, GUARD_RADIUS, 0)
        why = "return"
    end
    g.task_t = view.time
    -- (IsLocationTaskValid: the check time stays as it was - mNextTaskCheckTime - so a new place is looked at on
    -- arrival, then again 10..120 s later, kept half the time)
    print(string.format("classic_lua army.task id=%d mission=%d %s at=%d,%d units=%d", g.id, g.mission, why, g.x, g.y,
        #g.units))
end

-- The crossings (EECPASGSeaTransport: an attack sub-group whose target lies across water is carried there). DEVIATION:
-- the crossing is lib.navy's landing (board, sail, unload at the weakest beach of the target's land) with the free
-- transports; while too few hold the group, one more transport is ordered at a time (the epoch's sea transport,
-- ferry.lua's class), and a crossing not under way in 5 minutes, or aborted, gives the group a defence task, with no
-- crossing tried again for 10 minutes. The group's own orders wait while it crosses (lib.groups `manual`)
local CROSS_WAIT_MS, CROSS_AGAIN_MS = 300000, 600000

local function ships_in_use(memory)
    local used = {}
    for _, g in ipairs(G.all(army(memory))) do
        if g.crossing ~= nil and g.crossing.L ~= nil then
            for _, id in ipairs(g.crossing.L.ships) do
                used[id] = true
            end
        end
    end
    local f = memory.ferry
    if f ~= nil and f.trip ~= nil then
        for _, id in ipairs(f.trip.L.ships) do
            used[id] = true
        end
    end
    return used
end

local function group_centre(g)
    local sx, sy, n = 0, 0, 0
    for _, id in ipairs(g.units) do
        local u = R.unit(id)
        if u ~= nil and u.inside == nil then
            sx, sy, n = sx + u.x, sy + u.y, n + 1
        end
    end
    if n == 0 then
        return nil
    end
    return sx // n, sy // n
end

local function needs_crossing(view, memory, g)
    if g.sea or g.task ~= "attack" or not FERRY.water_map(memory) or view.time < (g.no_cross or 0) then
        return false
    end
    local cx, cy = group_centre(g)
    return cx ~= nil and not ai.reach(cx, cy, g.x, g.y)
end

local function end_crossing(view, memory, g, why)
    g.crossing = nil
    g.manual = false
    if why ~= nil then
        g.no_cross = view.time + CROSS_AGAIN_MS
        print(string.format("classic_lua army.cross id=%d end=%s", g.id, why))
        g.mission = 0
        assign_task(view, memory, g)
    end
end

local function cross_step(view, memory, g)
    local c = g.crossing
    if c == nil then
        c = {t = view.time}
        g.crossing = c
        g.manual = true
        print(string.format("classic_lua army.cross id=%d start units=%d to=%d,%d", g.id, #g.units, g.x, g.y))
    end
    if c.L == nil then
        if view.time - c.t >= CROSS_WAIT_MS then
            end_crossing(view, memory, g, "no_ships")
            return
        end
        local used = ships_in_use(memory)
        local ships, cap = {}, 0
        for _, t in ipairs(NAVY.transports()) do
            if not used[t.id] and t.cargo == 0 and cap < #g.units then
                ships[#ships + 1] = t
                cap = cap + t.cap
            end
        end
        if cap >= #g.units and #ships > 0 then
            local L, why = NAVY.plan({x = g.x, y = g.y, player = g.enemy or 0}, g.units, ships)
            if L == nil then
                end_crossing(view, memory, g, "plan_" .. tostring(why))
                return
            end
            c.L = L
            print(string.format("classic_lua army.cross id=%d board ships=%d units=%d", g.id, #ships, #g.units))
            return
        end
        local class = FERRY.transport_class(view)
        if class == nil then
            end_crossing(view, memory, g, "no_dock")
            return
        end
        if not U.is_requested(memory, class) and not U.in_production(view, class) then
            U.request(view, memory, class, 1)
        end
        return
    end
    local res, out = NAVY.landing_step(c.L)
    if res == "landed" then
        print(string.format("classic_lua army.cross id=%d landed n=%d", g.id, #out))
        end_crossing(view, memory, g, nil)
        set_place(g, "attack", g.x, g.y, g.radius, g.target)
    elseif res == "aborted" then
        end_crossing(view, memory, g, "aborted_" .. tostring(out))
    end
end

-- the sub-groups' life: wait for the slots (at the rally point), gather (30 s at most), the task; a task done is
-- given again
-- ---------------------------------------------------------------------------------------------------------------
-- The air sub-groups (EECPASGAir with its units EECPASGUAirBomber and EECPASGUAirFighter): the planes of a group.
-- A manual lib.groups group: no rally point (they take off from their airports and carriers when the slots are
-- filled: the Launch state), a task as a land sub-group's but within the planes' fuel reach (CanReach), the
-- targets round the task's place (the air target filter: no walls, runways or animals), each plane's own target
-- (ChooseTarget: one it can hurt, the armed first, its prejudices first: AssignPrejudices), fighters flying to
-- the target's tile (the engine's dogfight), the others attacking it; a plane with nothing to do flies to the task's
-- place again (from its base too: the engine brings it home when its fuel runs out). DEVIATIONS: the target score
-- is the port's (armed, prejudice, distance; not ScoreTarget's), the route is the engine's (no air route request)

local KIND_FIGHTER, KIND_FIGHTER2, KIND_BOMBER = 0xc, 0x17, 0xe
local AIR_SCAN = 15              -- the targets' reach round the task's place (tiles)
local AIR_RESEND_MS = 10000      -- a plane given nothing is sent to the task's place again after this
local AIR_KEEP2 = 100            -- EECPASGUAirBomber::Retarget: the current target kept while within 10 tiles

-- a plane's fuel reach in tiles from its base (CountUnitsInRange: flight seconds x speed x 0.9); nil: no limit
local function air_range(c)
    local ft = c.flight_time or 0
    if ft <= 0 or (c.speed or 0) <= 0 then
        return nil
    end
    return (ft // 1000) * c.speed * 9 // 1000
end

-- CanReach: an empty group, or a plane without a fuel limit, or one whose base lies within its reach of the place
local function air_reach(g, x, y)
    if #g.units == 0 then
        return true
    end
    local cols = ai.get(g.units, {"type", "x", "y", "inside", "base"})
    for k = 1, #g.units do
        local ty = cols.type[k]
        if ty ~= nil then
            local r = air_range(cls(ty))
            if r == nil then
                return true
            end
            local b = R.unit(cols.inside[k] or cols.base[k] or 0)
            if b ~= nil and d2(b.x, b.y, x, y) <= r * r then
                return true
            end
        end
    end
    return false
end

-- AssignPrejudices (on every new task): a bomber may prefer citizens (10%) or anti-air units (a chance of 15-40%
-- for the group), a fighter bombers (35-70%); each only when its weapon hurts that kind. Bits: 1 citizens, 2
-- anti-air, 4 bombers
local function air_prejudices(g)
    g.prej = {}
    local aa_k, bomber_k = ai.random(150, 400), ai.random(350, 700)
    for _, id in ipairs(g.units) do
        local u = R.unit(id)
        if u ~= nil then
            local k = cls(u.type)
            local bits = 0
            if not k.is_transport then
                local ak = k.ai_kind or 0
                if ak == KIND_FIGHTER or ak == KIND_FIGHTER2 then
                    if ai.can_hurt(u.type, "a_bomber_11_b_17") and T.random_k() <= bomber_k then
                        bits = bits | 4
                    end
                else
                    if ai.can_hurt(u.type, C.citizen) and T.random_k() <= 100 then
                        bits = bits | 1
                    end
                    if T.random_k() <= aa_k then
                        bits = bits | 2
                    end
                end
            end
            g.prej[tostring(id)] = bits
        end
    end
end

-- the air task: an attack (mission 0 or 2) the target's weakest territory within reach; else the defence ladder of
-- the planes (GetGuardChokepointPercentage: guard_chokepoint_air_percentage; the town: guard_town_percentage, at the
-- town as a defence task - DEVIATION: not in the town's aid package; the continent: always); else the rally point
local function assign_air_task(view, memory, g)
    local b = memory.t.behavior
    local why = nil
    if g.mission == 2 or g.mission == 0 then
        local regions = target_regions(memory, g.enemy)
        for i = #regions, 1, -1 do
            local pick = regions[i]
            if air_reach(g, pick.x, pick.y) and not (g.last_x == pick.x and g.last_y == pick.y) then
                g.mission = 2
                g.last_x, g.last_y = pick.x, pick.y
                set_place(g, "attack", pick.x, pick.y, 0, g.enemy)
                why = "territory"
                break
            end
        end
        if why == nil then
            g.mission = 0
        end
    end
    local x, y = g.cx or g.x, g.cy or g.y
    if x == nil or x < 0 then
        x, y = rally_point(view)
    end
    if why == nil and ai.choke_point ~= nil and T.random_k() < (b.guard_chokepoint_air_percentage_k or 0) then
        local cx, cy = ai.choke_point(x, y, 0, ai.random(0, 1 << 30), ai.random(1, 0x7fffffff))
        if cx ~= nil and air_reach(g, cx, cy) then
            g.mission = 1
            set_place(g, "defend", cx, cy, GUARD_RADIUS, 0)
            why = "choke"
        end
    end
    if why == nil and T.random_k() < b.guard_town_percentage_k then
        local tw = threatened_town(memory)
        if tw ~= nil then
            g.mission = 1
            set_place(g, "defend", ai.random(tw.l, tw.r), ai.random(tw.t, tw.b), GUARD_RADIUS, 0)
            why = "town"
        end
    end
    if why == nil then
        local px, py = continent_point(view, x, y)
        if px ~= nil and air_reach(g, px, py) then
            g.mission = 1
            set_place(g, "defend", px, py, GUARD_RADIUS, 0)
            why = "land"
        end
    end
    if why == nil then
        local rx, ry = rally_point(view)
        set_place(g, "defend", rx, ry, GUARD_RADIUS, 0)
        why = "return"
    end
    g.task_t = view.time
    g.task_check = view.time + ai.random(15, 180) * 1000
    g.sent = {}
    air_prejudices(g)
    print(string.format("classic_lua army.task id=%d mission=%d %s at=%d,%d units=%d air", g.id, g.mission, why, g.x,
        g.y, #g.units))
end

-- IsLocationTaskValid (every 15-180 s): an attack while the target's known units or buildings stand within 2
-- regions of the place; a defence lapses 35% of the time
local function air_task_valid(view, memory, g)
    if view.time < (g.task_check or 0) then
        return true
    end
    local ok
    if g.task == "attack" then
        ok = #ai.memory{player = g.enemy, near = {g.x, g.y, REGION * 2}, limit = 1} > 0
            or #ai.seen{enemy = true, player = g.enemy, near = {g.x, g.y, REGION * 2}} > 0
    else
        ok = T.random_k() <= 650
    end
    if ok then
        g.task_check = view.time + ai.random(15, 180) * 1000
    end
    return ok
end

-- the air target filter round the place: enemies seen, no walls (towers are), runways, resources or animals
local function air_targets(g)
    local f = {enemy = true, near = {g.x, g.y, AIR_SCAN}}
    if g.task == "attack" and g.enemy ~= 0 then
        f.player = g.enemy
    end
    local out = {}
    for _, id in ipairs(ai.seen(f)) do
        local u = ai.unit(id)
        if u ~= nil then
            local cat = u.category
            local t = u.type or ""
            if cat ~= "resource" and cat ~= "animal" and cat ~= "ambient" and cat ~= "projectile"
                    and string.sub(t, 1, 7) ~= "b_wall_" and string.find(t, "runway", 1, true) == nil then
                out[#out + 1] = u
            end
        end
    end
    table.sort(out, function(a, b2) return a.id < b2.id end)
    return out
end

-- ChooseTarget for one plane: a target its weapon hurts; its prejudices first, then the armed, then the nearest
local function air_choose(u, k, bits, targets, hurts)
    local best, bs = nil, nil
    for _, t in ipairs(targets) do
        local key = u.type .. ">" .. t.type
        local h = hurts[key]
        if h == nil then
            h = ai.can_hurt(u.type, t.type)
            hurts[key] = h
        end
        if h then
            local tk = cls(t.type)
            local pref = ((bits & 1) ~= 0 and t.category == "citizen")
                or ((bits & 2) ~= 0 and (tk.ai_kind or 0) == KIND_ANTI_AIR)
                or ((bits & 4) ~= 0 and (tk.ai_kind or 0) == KIND_BOMBER)
            local s = (pref and 0 or 2000000) + (t.armed and 0 or 1000000) + d2(u.x, u.y, t.x, t.y)
            if bs == nil or s < bs then
                best, bs = t, s
            end
        end
    end
    return best
end

-- the planes' orders (EECPASGUAirBomber / EECPASGUAirFighter::Update, AttackWith, SendUnitTo)
local function air_step(view, memory, g)
    local targets = air_targets(g)
    local hurts = {}
    local fighting = 0
    local attacks, moves, sends = {}, {}, {}
    g.sent = g.sent or {}
    g.prej = g.prej or {}
    for _, id in ipairs(g.units) do
        local u = ai.unit(id)
        if u ~= nil then
            local k = cls(u.type)
            local ak = k.ai_kind or 0
            local fighter = g.space or ak == KIND_FIGHTER or ak == KIND_FIGHTER2
            local key = tostring(id)
            local aim = g.aims[id]
            local cur = aim ~= nil and ai.unit(aim) or nil
            local pick = nil
            -- a bomber keeps a live target within 10 tiles
            if cur ~= nil and not fighter and u.inside == nil and d2(u.x, u.y, cur.x, cur.y) <= AIR_KEEP2 then
                pick = cur
            elseif #targets > 0 then
                pick = air_choose(u, k, g.prej[key] or 0, targets, hurts)
            end
            if pick ~= nil then
                fighting = fighting + 1
                if aim ~= pick.id or u.action == "idle" or u.action == "inside" then
                    g.aims[id] = pick.id
                    if fighter then
                        local mk = pick.x * 10000 + pick.y
                        moves[mk] = moves[mk] or {}
                        table.insert(moves[mk], id)
                    else
                        attacks[pick.id] = attacks[pick.id] or {}
                        table.insert(attacks[pick.id], id)
                    end
                end
            else
                g.aims[id] = nil
                if (u.action == "idle" or u.action == "inside") and view.time >= (g.sent[key] or 0) then
                    g.sent[key] = view.time + AIR_RESEND_MS
                    sends[#sends + 1] = id
                end
            end
        end
    end
    for _, tid in ipairs(ai.keys(attacks)) do
        ai.attack(attacks[tid], tid)
    end
    for _, mk in ipairs(ai.keys(moves)) do
        ai.move(moves[mk], mk // 10000, mk % 10000, {forced = true})
    end
    if #sends > 0 and g.x >= 0 then
        ai.move(sends, g.x, g.y, {forced = g.mission ~= 1})
    end
    g.state = fighting > 0 and "fighting" or "air"
end

-- the air sub-group's life: wait for the slots (the deadline cancels the rest), launch with a task, then the orders,
-- a new task when the old one lapses
local function air_update(view, memory, g)
    if g.phase == "wait" then
        if #g.slots == 0 and #g.units > 0 then
            g.phase, g.phase_t = "task", view.time
            assign_air_task(view, memory, g)
        end
        return
    end
    if g.phase ~= "task" or #g.units == 0 then
        return
    end
    if g.state ~= "fighting" and not air_task_valid(view, memory, g) then
        assign_air_task(view, memory, g)
    end
    air_step(view, memory, g)
end

local function update_groups(view, memory)
    local mem = army(memory)
    -- (the transports the crossings hold, for the ferries)
    memory.cross_ships = {}
    for _, g in ipairs(G.all(mem)) do
        if g.crossing ~= nil and g.crossing.L ~= nil then
            for _, id in ipairs(g.crossing.L.ships) do
                memory.cross_ships[id] = true
            end
        end
    end
    for _, g in ipairs(G.all(mem)) do
        if g.role == "army" and g.air then
            air_update(view, memory, g)
        elseif g.role == "army" then
            if g.phase == "wait" then
                if #g.slots == 0 then
                    g.phase, g.phase_t = "gather", view.time
                end
            elseif g.phase == "gather" then
                local rx, ry = rally_point(view)
                local all = true
                for _, id in ipairs(g.units) do
                    local u = R.unit(id)
                    if u ~= nil and rx ~= nil and d2(u.x, u.y, rx, ry) > 144 then
                        all = false
                    end
                end
                if #g.units > 0 and (all or view.time - g.phase_t >= GATHER_MS) then
                    g.phase = "task"
                    assign_task(view, memory, g)
                end
            elseif g.phase == "task" and #g.units > 0 and (g.crossing ~= nil or needs_crossing(view, memory, g)) then
                cross_step(view, memory, g)
            elseif g.phase == "task" and #g.units > 0 then
                -- (EECPASGLand: a sub-group whose fight is over goes idle and is given a task again)
                local fought = g.was_fighting and g.state ~= "fighting"
                g.was_fighting = g.state == "fighting"
                if fought and not g.sea and g.mission ~= 3 then
                    assign_task(view, memory, g)
                elseif g.task == "attack" and g.state == "arrived" then
                    assign_task(view, memory, g)
                elseif g.task == "defend" and g.mission ~= 3 and g.state ~= "fighting"
                        and (g.sea or g.state == "arrived") and view.time >= (g.task_check or 0) then
                    -- IsLocationTaskValid: at its place (and its check time past) a defence task is dropped half
                    -- the time, else looked at again in 10-120 s (a sea sub-group's place: done once looked at)
                    if g.sea or ai.random(0, 1) == 0 then
                        assign_task(view, memory, g)
                    else
                        g.task_check = view.time + ai.random(10, 120) * 1000
                    end
                end
            end
        end
    end
end

-- DropSpentGroups: the groups without units and without open slots
local function drop_spent(view, memory)
    local a = army(memory)
    if view.time < a.next_drop then
        return
    end
    a.next_drop = view.time + DROP_MS
    for _, g in ipairs(G.all(a)) do
        if g.role == "army" and #g.units == 0 and #(g.slots or {}) == 0 then
            a.groups[g.id] = nil
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The defence commander

-- CheckNaval: every ship_maintenance_timer minutes, with more than 50 water tiles on the map, the warships (own and
-- asked for) topped up to the difficulty's ships_to_maintain by a group of a sea-only strategy (kind 5: a defence
-- group) against a random enemy; the peace rules hold it back as an attack's. ChooseUnitsToKill's room: NOT STARTED.
-- The carrier fighters: from WW2, the difficulty's number kept (carrier_fighters_*)
local function check_naval(view, memory)
    local a = army(memory)
    if view.time <= (a.next_naval or 0) then
        return
    end
    local t = memory.t
    local b = t.behavior
    if (t.water_tiles or 0) > 50 then
        local limit
        if t.deathmatch then
            limit = T.by_difficulty(memory, b.ships_to_maintain_dm_easy, b.ships_to_maintain_dm_medium,
                b.ships_to_maintain_dm_hard)
        else
            limit = T.by_difficulty(memory, b.ships_to_maintain_easy, b.ships_to_maintain_medium,
                b.ships_to_maintain_hard)
        end
        local have = #own_ships()
        for _, g in ipairs(G.all(a)) do
            for _, sl in ipairs(g.slots or {}) do
                if domain_of(cls(sl.c)) == "sea" then
                    have = have + 1
                end
            end
        end
        if have < limit then
            local target = random_enemy()
            local p = t.personality
            local peace = (view.epoch <= 0 and not p.prehistoric_attacks) or (view.epoch == 1 and not p.stone_attacks)
            if target ~= nil and not peace then
                create_group(view, memory, 5, 1, target, "naval", limit - have)
            end
        end
        -- the carrier fighters (from the Atomic Age - WW2): the difficulty's number kept, own and asked for, of the
        -- epoch's carrier fighter (WW2 the Corsair, Modern the F-14, later the Avenger)
        if view.epoch >= 10 then
            local wanted
            if t.deathmatch then
                wanted = T.by_difficulty(memory, b.carrier_fighters_dm_easy, b.carrier_fighters_dm_medium,
                    b.carrier_fighters_dm_hard)
            else
                wanted = T.by_difficulty(memory, b.carrier_fighters_easy, b.carrier_fighters_medium,
                    b.carrier_fighters_hard)
            end
            wanted = (wanted or 0) - #ai.units{type = FIGHTERS}
            for _, name in ipairs(FIGHTERS) do
                wanted = wanted - (U.state(memory).waiting[name] or 0)
            end
            if wanted > 0 then
                local class = view.epoch == 10 and "a_carrierfighter11_corsair1"
                    or view.epoch == 11 and "a_carrierfighter12_f_14" or "a_carrierfighter13_avenger"
                U.request(view, memory, class, wanted)
            end
        end
    end
    a.next_naval = view.time + b.ship_maintenance_timer * 60000
end

-- CheckSpace: every ship_maintenance_timer minutes on a space map, the space units (own and asked for: armed space
-- classes, no transport) topped up to ships_to_maintain (the warships' number, as the C++ reads it) by a space-only
-- group (kind 5) against a random enemy; and a space carrier more while fewer than the space docks and 4.
-- ChooseUnitsToKill's room: NOT STARTED (as CheckNaval's)
local function check_space(view, memory)
    local a = army(memory)
    if view.time <= (a.next_space or 0) then
        return
    end
    local t = memory.t
    local b = t.behavior
    if t.space then
        local limit
        if t.deathmatch then
            limit = T.by_difficulty(memory, b.ships_to_maintain_dm_easy, b.ships_to_maintain_dm_medium,
                b.ships_to_maintain_dm_hard)
        else
            limit = T.by_difficulty(memory, b.ships_to_maintain_easy, b.ships_to_maintain_medium,
                b.ships_to_maintain_hard)
        end
        local have = 0
        for _, id in ipairs(ai.units{}) do
            local u = R.unit(id)
            if u ~= nil then
                local k = cls(u.type)
                if k.domain == "space" and not k.is_transport and not k.building and k.category ~= "citizen" and k.category ~= "boat" then
                    have = have + 1
                end
            end
        end
        for _, g in ipairs(G.all(a)) do
            for _, sl in ipairs(g.slots or {}) do
                if domain_of(cls(sl.c or sl)) == "space" then
                    have = have + 1
                end
            end
        end
        if have < limit then
            local target = random_enemy()
            if target ~= nil then
                create_group(view, memory, 5, 1, target, "space", limit - have, "space")
            end
        end
        local carriers = #ai.units{type = SPACE_CARRIER} + (U.state(memory).waiting[SPACE_CARRIER] or 0)
        if carriers < #ai.units{type = "b_space_dock"} and carriers < 4 then
            U.request(view, memory, SPACE_CARRIER, 1)
        end
    end
    a.next_space = view.time + b.ship_maintenance_timer * 60000
end

-- CounterAttack: a defence group (kind 1, mission 3) for the package's town against its attacker
local function counter_attack(view, memory, pk)
    if pk.player == 0 or unit_allowance(view, memory) <= 0 then
        return false
    end
    local tw = town_by_id(pk.town)
    if tw == nil then
        return false
    end
    local g = create_group(view, memory, 1, 3, pk.player, "counter")
    if g == nil then
        return false
    end
    g.pkg = pk.town
    g.task_town = pk.town
    pk.groups[#pk.groups + 1] = g.id
    return true
end

-- SendBestSubGroup: a free land sub-group (not attacking, in no package) best matching the net threat (else the
-- nearest) goes to the town (mission 3)
local function send_best(view, memory, pk)
    local a = army(memory)
    local tw = town_by_id(pk.town)
    if tw == nil then
        return false
    end
    local inpkg = {}
    for _, key in ipairs(ai.keys(a.packages)) do
        for _, id in ipairs(a.packages[key].groups) do
            inpkg[id] = true
        end
    end
    local _, points = worst_threats(memory, tw)
    local net = 0
    for _, p in ipairs(points) do
        net = p.enemy - p.own   -- (the last point's: the C++ assigns, it does not sum)
    end
    local best, bd = nil, nil
    for _, g in ipairs(G.all(a)) do
        if g.role == "army" and not g.sea and not g.air and g.mission ~= 2 and not inpkg[g.id] and #g.units > 0 and g.phase == "task" then
            local d
            if net > 0 then
                local s = 0
                for _, id in ipairs(g.units) do
                    local u = R.unit(id)
                    if u ~= nil then
                        s = s + (cls(u.type).influence or 0)
                    end
                end
                d = math.abs(net - s)
            else
                d = d2(g.cx or g.x, g.cy or g.y, tw.cx, tw.cy)
            end
            if bd == nil or d < bd then
                best, bd = g, d
            end
        end
    end
    if best == nil then
        return false
    end
    best.mission = 3
    best.pkg = pk.town
    pk.groups[#pk.groups + 1] = best.id
    set_place(best, "defend", tw.cx, tw.cy, GUARD_RADIUS, 0)
    print(string.format("classic_lua army.aid id=%d town=%d units=%d", best.id, pk.town, #best.units))
    return true
end

-- AssignSubGroups: a free sub-group (unless counter-attack only), else a counter-attack group
local function assign_subgroups(view, memory, pk, counter_only)
    if not counter_only and send_best(view, memory, pk) then
        return true
    end
    return counter_attack(view, memory, pk)
end

-- SendAid: the town's package (made when there is none: its threat, its attacker or a random enemy)
local function send_aid(view, memory, tw, counter_only)
    local a = army(memory)
    local pk = a.packages[tw.id]
    if pk ~= nil then
        return assign_subgroups(view, memory, pk, counter_only)
    end
    local threat, player = town_threat(memory, tw, false)
    pk = {town = tw.id, threat = threat, player = player or 0, groups = {}, state = "recalled", next_update = 0,
          next_send = 0}
    if pk.player == 0 then
        pk.player = random_enemy() or 0
    end
    local ok = false
    if pk.player ~= 0 then
        ok = assign_subgroups(view, memory, pk, counter_only)
    end
    if ok then
        a.packages[tw.id] = pk
    end
    return ok
end

local function town_guard_delay(memory)
    local b = memory.t.behavior
    if memory.t.deathmatch then
        return T.by_difficulty(memory, b.dm_town_guard_update_easy, b.dm_town_guard_update_medium,
            b.dm_town_guard_update_hard)
    end
    return T.by_difficulty(memory, b.town_guard_update_easy, b.town_guard_update_medium, b.town_guard_update_hard)
end

-- Deploy: the package's groups near the town go to the threat points in turn (a combat task there)
local function deploy(view, memory, pk, tw, threats)
    if #threats == 0 then
        return
    end
    local a = army(memory)
    local sent = 0
    for k = #pk.groups, 1, -1 do
        local g = a.groups[pk.groups[k]]
        if g ~= nil and not g.sea and not g.air and #g.units > 0 and g.phase ~= "wait" then
            local gx, gy = g.cx or g.x, g.cy or g.y
            local cx, cy = hull_closest(tw, gx, gy)
            local dist = d2(cx, cy, gx, gy)
            local skip = dist > DEPLOY_RANGE2 and not hull_contains(tw, gx, gy)
            if not skip and dist <= DEPLOY_NEAR2 then
                for _, p in ipairs(threats) do
                    if p.x == cx and p.y == cy then
                        skip = true
                    end
                end
            end
            if not skip and g.combat then
                for _, p in ipairs(threats) do
                    if p.x == g.x and p.y == g.y then
                        skip = true
                    end
                end
            end
            if not skip then
                local p = threats[sent % #threats + 1]
                g.combat = true
                g.phase = "task"
                set_place(g, "defend", p.x, p.y, GUARD_RADIUS + 6, 0)
                sent = sent + 1
            end
        end
    end
    if sent > 0 then
        print(string.format("classic_lua army.deploy town=%d groups=%d threats=%d", pk.town, sent, #threats))
    end
end

-- Gather: more groups while the worst threats outnumber them (own towns: twice, with a net threat), then Deploy
local function gather(view, memory, pk, tw)
    local threats, points = worst_threats(memory, tw)
    if #threats == 0 then
        return
    end
    local sent = false
    if #pk.groups < #threats then
        if pk.next_send <= view.time then
            assign_subgroups(view, memory, pk, false)
            sent = true
        end
    elseif #pk.groups < #threats * 2 then
        local total = 0
        for _, p in ipairs(points) do
            total = total + p.enemy - p.own
        end
        if total > 0 and pk.next_send <= view.time then
            assign_subgroups(view, memory, pk, false)
            sent = true
        end
    end
    deploy(view, memory, pk, tw, threats)
    if sent then
        pk.next_send = view.time + town_guard_delay(memory)
    end
end

-- Recall: aid groups (mission 3) keep their combat task; the others drop theirs (a new task)
local function recall(view, memory, pk)
    local a = army(memory)
    for _, id in ipairs(pk.groups) do
        local g = a.groups[id]
        if g ~= nil and #g.units > 0 then
            g.combat = false
            if g.mission ~= 3 and g.phase == "task" then
                assign_task(view, memory, g)
            end
        end
    end
end

-- UpdatePackages: each due package (recalled: gathers once the threat passes aid_threat_threshold; gathering:
-- recalls below it, else gathers again); finished packages go
local function update_packages(view, memory)
    local a = army(memory)
    local thr = memory.t.behavior.aid_threat_threshold_k
    for _, key in ipairs(ai.keys(a.packages)) do
        local pk = a.packages[key]
        -- DropDeadSubGroups
        local keep = {}
        for _, id in ipairs(pk.groups) do
            if a.groups[id] ~= nil then
                keep[#keep + 1] = id
            end
        end
        pk.groups = keep
        local tw = town_by_id(pk.town)
        if view.time >= pk.next_update then
            local delay = 35000
            if #pk.groups > 0 then
                if tw == nil then
                    for _, id in ipairs(pk.groups) do
                        local g = a.groups[id]
                        if g ~= nil then
                            g.mission, g.pkg = 1, nil
                        end
                    end
                    pk.groups, pk.next_send, pk.threat = {}, 0, 0
                else
                    pk.threat = town_threat(memory, tw, false)
                    if pk.state == "recalled" then
                        if pk.threat > thr then
                            pk.state = "gathering"
                            gather(view, memory, pk, tw)
                            delay = 20000
                        end
                    else
                        if pk.threat <= thr then
                            pk.state = "recalled"
                            recall(view, memory, pk)
                        else
                            gather(view, memory, pk, tw)
                            delay = 20000
                        end
                    end
                end
            end
            pk.next_update = view.time + delay
        end
        local units = 0
        for _, id in ipairs(pk.groups) do
            units = units + #a.groups[id].units + #(a.groups[id].slots or {})   -- (a waiting group counts its slots)
        end
        local finished = not (pk.threat > thr and #pk.groups > 0)
        if finished and units == 0 and pk.next_send <= view.time then
            for _, id in ipairs(pk.groups) do
                a.groups[id].pkg = nil
            end
            a.packages[key] = nil
        end
    end
end

-- CreateDefendGroup: a defence group (kind 1, mission 1) against a random enemy
local function create_defend_group(view, memory)
    local p = random_enemy()
    if p ~= nil and unit_allowance(view, memory) > 0 then
        create_group(view, memory, 1, 1, p, "defend")
    end
end

-- PlanAid: with units allowed, by the town-guard chance aid to the most threatened own town (counter-attack only;
-- unthreatened: one of the 3 largest), else a defence group; the next in defensive_unit_update
local function plan_aid(view, memory)
    local cp = memory.cp
    local b = memory.t.behavior
    if unit_allowance(view, memory) > 0 then
        if T.random_k() < b.town_guard_percentage_k then
            local tw, threat = threatened_town(memory)
            if tw ~= nil then
                if threat > 0 then
                    send_aid(view, memory, tw, true)
                else
                    local list = {}
                    for _, t in ipairs(towns()) do
                        list[#list + 1] = t
                    end
                    table.sort(list, function(p, q)
                        if p.value ~= q.value then
                            return p.value > q.value
                        end
                        return p.id < q.id
                    end)
                    while #list > 3 do
                        list[#list] = nil
                    end
                    if T.random_k() <= 250 then
                        shuffle(list)
                    end
                    send_aid(view, memory, list[1], true)
                end
            end
        else
            create_defend_group(view, memory)
        end
    end
    local delay
    if memory.t.deathmatch then
        delay = T.by_difficulty(memory, b.dm_defensive_unit_update_easy, b.dm_defensive_unit_update_medium,
            b.dm_defensive_unit_update_hard)
    else
        delay = T.by_difficulty(memory, b.defensive_unit_update_easy, b.defensive_unit_update_medium,
            b.defensive_unit_update_hard)
    end
    cp.next_aid = view.time + delay
end

-- EECPDefenseCommander::Update (every 8 s, or the packages' next update): packages, own towns' aid, PlanAid
local function defense(view, memory)
    local cp = memory.cp
    local a = army(memory)
    local b = memory.t.behavior
    if cp.next_aid == nil then
        -- (the first PlanAid after the replenishment delay)
        cp.next_aid = view.time + T.random(b.replenish_delay_min, b.replenish_delay_max) * 60000
    end
    if view.time < a.next_defense or not cp.defense_enabled or R.town(view) == nil then
        return
    end
    a.next_defense = view.time + 8000
    update_packages(view, memory)
    -- AidOwnTowns
    local list = towns()
    for k = #list, 1, -1 do
        if a.packages[list[k].id] == nil then
            send_aid(view, memory, list[k], false)
        end
    end
    if view.time >= cp.next_aid then
        plan_aid(view, memory)
    end
    check_naval(view, memory)
    check_space(view, memory)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The attack (EECPCombatManager::Update when IsAttackDue)

local function attack(view, memory)
    local cp = memory.cp
    local a = army(memory)
    if R.town(view) == nil or view.time < cp.next_attack then
        return
    end
    local main = require("classic_lua.main")
    local b = memory.t.behavior
    local humans, computers = enemies()
    local list = computers
    if #humans > 0 and #computers > 0 and T.random_k() < b.computer_aggression_k // 100 then   -- (mComputerAggression x 0.01: 10 -> 10%)
        list = computers
    elseif #humans > 0 then
        list = humans
    end
    if #list > 0 then
        -- theLargest: the enemy whose land overlaps the own (DEVIATION: the nearest known building within the own
        -- reach), else the nearest; theAided: the enemy that did the most damage
        local tx, ty = R.town(view)
        local largest, ld = nil, nil
        for _, p in ipairs(list) do
            local f = file_of(memory, p)
            for _, bld in ipairs(f.buildings or {}) do
                local d = d2(bld.x, bld.y, tx, ty)
                if ld == nil or d < ld or (d == ld and p < largest) then
                    largest, ld = p, d
                end
            end
        end
        if largest == nil then
            for _, e in ipairs(view.enemies or {}) do
                for _, p in ipairs(list) do
                    if e.player == p and (e.town_x or -1) >= 0 and largest == nil then
                        largest = p
                    end
                end
            end
        end
        local aided, most = nil, 0
        for _, p in ipairs(list) do
            local dmg = a.damage[p] or 0
            if dmg > 0 and (aided == nil or dmg > most) then
                aided, most = p, dmg
            end
        end
        -- theWeakest (the enemy short of a resource the player needs): NOT STARTED, fog
        local second = b.attack_to_claim_resource_chance + b.attack_enemy_chance
        local roll = T.random_k()
        local choice
        if largest ~= nil then
            choice = (aided ~= nil and roll > second * 1000) and aided or largest
        else
            choice = aided
        end
        if choice == nil then
            choice = list[ai.random(1, #list)]
        end
        local p = memory.t.personality
        local peace = (view.epoch <= 0 and not p.prehistoric_attacks) or (view.epoch == 1 and not p.stone_attacks)
        if not peace then
            create_group(view, memory, 0, 2, choice, "attack")
        end
    end
    main.set_next_attack(view, memory)
end

-- NoteThreat: the own citizens lost per 9-tile region (where they were last hit); past 20 the nearest army group
-- not attacking goes there (CombatManager::Attack on the own tile)
local function note_deaths(view, memory, events)
    local a = army(memory)
    for _, e in ipairs(events) do
        if e.kind == "attacked" then
            local u = ai.unit(e.unit)
            if u ~= nil and u.category == "citizen" then
                a.hits[e.unit] = {x = u.x, y = u.y, t = view.time}
            end
            local by = e.attacker ~= nil and ai.unit(e.attacker) or nil
            if by ~= nil and by.player ~= nil and by.player ~= memory.me then
                a.damage[by.player] = (a.damage[by.player] or 0) + (e.damage or 0)
            end
        elseif e.kind == "died" then
            local h = a.hits[e.unit]
            if h ~= nil then
                a.hits[e.unit] = nil
                local key = (h.x // REGION) * 1000 + h.y // REGION
                a.deaths[key] = (a.deaths[key] or 0) + 1
                if a.deaths[key] > CITIZEN_DEATHS then
                    a.deaths[key] = nil
                    local best, bd = nil, nil
                    for _, g in ipairs(G.all(a)) do
                        if g.role == "army" and not g.sea and not g.air and g.mission ~= 2 and #g.units > 0 and g.phase == "task" then
                            local d = d2(g.cx or g.x, g.cy or g.y, h.x, h.y)
                            if bd == nil or d < bd then
                                best, bd = g, d
                            end
                        end
                    end
                    if best ~= nil then
                        set_place(best, "attack", h.x, h.y, 0, 0)
                        best.task_check = view.time + 60000
                        print(string.format("classic_lua army.respond id=%d at=%d,%d", best.id, h.x, h.y))
                    end
                end
            end
        end
    end
    -- old hits go
    if view.time % 60000 < 1000 then
        for _, id in ipairs(ai.keys(a.hits)) do
            if view.time - a.hits[id].t > 60000 then
                a.hits[id] = nil
            end
        end
    end
end

function M.on_events(view, memory, events)
    note_deaths(view, memory, events)
end

-- every step (EECPCombatManager::Tick)
-- KillUnitsForMilitary (EECPPersonnel::Update, every 30 s) through ChooseUnitsToKill: the citizens over the citizen
-- share of the population limit (GetCitizenSurplus: the citizens and boats, own and asked for, against
-- ceil(limit x citizen_percentage)) are deleted, the idle first (10070), then the waiting gatherers (10060) and the
-- gatherers (10000) by their resource (wood 40, stone 30, iron 20, gold 15, food 10). The transports' rule (5 more
-- at the limit with too few transports): NOT STARTED (the transports)
local KILL_MS = 30000
local KILL_SCORE = {wood = 40, stone = 30, iron = 20, gold = 15, food = 10}

local function kill_for_military(view, memory)
    local a = army(memory)
    if view.time < (a.next_kill or 0) then
        return
    end
    a.next_kill = view.time + KILL_MS
    local cap = view.population_cap
    if cap <= 0 then
        return
    end
    local ids = ai.units{category = "citizen"}
    for _, id in ipairs(ai.units{category = "boat"}) do
        ids[#ids + 1] = id
    end
    local um = U.state(memory)
    local asked = 0
    for _, c in ipairs({C.citizen, C.female_citizen}) do
        asked = asked + (um.waiting[c] or 0)
    end
    local surplus = #ids + asked - (cap * memory.t.behavior.citizen_percentage_k + 999) // 1000
    if surplus <= 0 or #ids == 0 then
        return
    end
    local cols = ai.get(ids, {"action", "gathers", "waiting", "inside", "category"})
    local list = {}
    for k = 1, #ids do
        if cols.action[k] ~= nil and cols.inside[k] == nil then
            local score = 10000
            if cols.action[k] == "idle" then
                score = 10070
            elseif cols.action[k] == "gather" then
                if cols.waiting[k] then
                    score = 10060
                end
                score = score + (KILL_SCORE[cols.gathers[k] or ""] or 0)
            end
            list[#list + 1] = {id = ids[k], score = score}
        end
    end
    table.sort(list, function(p, q)
        if p.score ~= q.score then
            return p.score > q.score
        end
        return p.id < q.id
    end)
    local kill = {}
    for k = 1, math.min(surplus, #list) do
        kill[#kill + 1] = list[k].id
    end
    if #kill > 0 then
        ai.delete(kill)
        print(string.format("classic_lua army.kill citizens=%d surplus=%d", #kill, surplus))
    end
end

function M.step(view, memory)
    local a = army(memory)
    G.init(a)
    kill_for_military(view, memory)
    fill_requests(view, memory)
    update_groups(view, memory)
    if view.time >= (memory.me + 5) * 1000 then
        defense(view, memory)   -- (the defence commander's first update at the player id + 5 s)
    end
    attack(view, memory)
    drop_spent(view, memory)
    -- (a sub-group waiting for its first units is no lib.groups group yet: lib.groups drops empty groups)
    local parked, waiting = {}, {}
    for _, g in ipairs(G.all(a)) do
        if #(g.slots or {}) > 0 then
            if #g.units == 0 then
                parked[#parked + 1] = g
                a.groups[g.id] = nil
            else
                waiting[#waiting + 1] = g
            end
        end
    end
    -- (the soldiers the militia sent to fight are left to it)
    G.committed = {}
    local mi = memory.militia
    if mi ~= nil and mi.fight ~= nil then
        for _, id in ipairs(ai.keys(mi.fight)) do
            if view.time < mi.fight[id] then
                G.committed[id] = true
            else
                mi.fight[id] = nil
            end
        end
    end
    G.step(view, a)
    -- (a waiting group whose last units died in the step keeps its slots)
    for _, g in ipairs(waiting) do
        if a.groups[g.id] == nil then
            g.units, g.aims = {}, {}
            parked[#parked + 1] = g
        end
    end
    for _, g in ipairs(parked) do
        a.groups[g.id] = g
    end
    if view.time % 60000 < 1000 then
        local n, units, slots = 0, 0, 0
        for _, g in ipairs(G.all(a)) do
            n = n + 1
            units = units + #g.units
            slots = slots + #(g.slots or {})
        end
        print(string.format("classic_lua army groups=%d units=%d slots=%d packages=%d allowance=%d room=%d", n, units,
            slots, #ai.keys(a.packages), unit_allowance(view, memory), units_available(view, memory)))
    end
end

return M

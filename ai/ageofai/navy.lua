-- Age of AI port, the navy (upstream dll/ai/landing.c the landing, fleet.c the warships, navy.c the account and
-- the transports; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.navy"). On lib.navy (docs/AI_LUA_UNITS.md L6).
--
-- The navy acts when the enemy's buildings known lie on another land mass than the base (ai.reach false): a dock,
-- transports (the newest type the dock makes, up to landing_transports, as many as the soldiers at home need), and
-- the landing: at the attack time, when the force at home is 1.4 x what holds the beach (the enemy known within 20
-- tiles of the site) it boards and sails (lib.navy), lands and fights ashore as a group (lib.groups attack on the
-- target's buildings); a quarter left, or nothing more to fight: it is released, the transports go home. Warships:
-- navy_share of the military account buys up to a few, which guard the dock's water and hunt enemy boats.
local V = require("lib.view")
local G = require("lib.groups")
local N = require("lib.navy")
local S = require("ageofai.settings")
local M = {}

local SG_FOE = 20

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- the enemy known across the water: the nearest known enemy building whose land mass differs from the town's
local function target_across(view)
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        return nil
    end
    local list = ai.memory{buildings = true, near = {tx, ty, 300}, limit = 12}
    for _, m in ipairs(list) do
        if not ai.reach(tx, ty, m.x, m.y) then
            return m
        end
    end
    return nil
end

local function enemy_at(x, y, r)
    local str = 0
    for _, m in ipairs(ai.memory{near = {x, y, r}}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            str = str + m.hp * c.attack
        end
    end
    return str
end

-- the transport class the dock makes (the newest)
local function transport_class(view)
    local best = nil
    for _, c in ipairs(view.costs) do
        if not c.building and c.producer == "b_dock" then
            local cls = ai.class(c.type)
            if cls ~= nil and (cls.transport or 0) > 0 and (best == nil or c.type > best.type) then
                best = c
            end
        end
    end
    return best
end

local function warship_class(view)
    local best, bs = nil, 0
    for _, c in ipairs(view.costs) do
        if not c.building and c.military and c.producer == "b_dock" then
            local cls = ai.class(c.type)
            if cls ~= nil and (cls.attack or 0) > 0 and (cls.transport or 0) == 0 then
                local s = (cls.hp or 0) * cls.attack
                if s > bs then
                    best, bs = c, s
                end
            end
        end
    end
    return best
end

function M.step(view, memory, orders, budget, force)
    memory.navy = memory.navy or {}
    local mem = memory.navy
    local function order(o)
        orders[#orders + 1] = o
    end
    if not S.landing then
        return
    end
    local target = target_across(view)
    mem.across = target ~= nil
    -- nothing of the enemy known and the land explored: a boat looks over the water (the landing's warships look at
    -- the beach first upstream; here the exploration itself, since nothing crosses the water otherwise)
    local known = ai.memory{buildings = true, limit = 1}
    if #known == 0 and view.time >= 300000 and view.time - (mem.sea_scout_t or 0) >= 120000 then
        local scout = mem.sea_scout ~= nil and ai.unit(mem.sea_scout) or nil
        if scout == nil then
            local ships = ai.units{category = "ship", armed = true, limit = 1}
            if #ships == 0 then
                ships = ai.units{category = "boat", limit = 1}
            end
            if #ships > 0 then
                mem.sea_scout = ships[1]
                scout = ai.unit(ships[1])
            end
        end
        if scout ~= nil then
            ai.take(scout.id)
            -- the nearest unexplored point of its water body, furthest from the town first; one reached is skipped
            mem.sea_skips = mem.sea_skips or {}
            if mem.sea_goal_x ~= nil and d2(scout.x, scout.y, mem.sea_goal_x, mem.sea_goal_y) <= 16 and #mem.sea_skips < 64 then
                mem.sea_skips[#mem.sea_skips + 1] = {mem.sea_goal_x, mem.sea_goal_y}
            end
            local gx, gy = ai.unexplored(scout.x, scout.y, {sea = true, grid = 8, away = {view.town_x, view.town_y}, skip = mem.sea_skips})
            if gx == nil then
                -- the water body's grid ran dry (its id differs from the boat's tile's near the coast): the unexplored
                -- tiles of a coarse grid the boat can sail to (ai.route over the sea knows the map under the fog)
                local map = ai.map()
                local best, bd = nil, nil
                for y = 6, map.height - 6, 12 do
                    for x = 6, map.width - 6, 12 do
                        local t = ai.tile(x, y)
                        if t ~= nil and not t.explored then
                            local skip = false
                            for _, sk in ipairs(mem.sea_skips) do
                                if d2(sk[1], sk[2], x, y) <= 64 then
                                    skip = true
                                end
                            end
                            if not skip then
                                local d = d2(x, y, scout.x, scout.y) // 4 - d2(x, y, view.town_x, view.town_y)
                                if bd == nil or d < bd then
                                    best, bd = {x, y}, d
                                end
                            end
                        end
                    end
                end
                if best ~= nil and ai.route(scout.x, scout.y, best[1], best[2], {sea = true, avoid = 0}) ~= nil then
                    gx, gy = best[1], best[2]
                elseif best ~= nil then
                    mem.sea_skips[#mem.sea_skips + 1] = best   -- no water way there
                end
            end
            if gx ~= nil then
                mem.sea_goal_x, mem.sea_goal_y = gx, gy
                ai.move(scout.id, gx, gy, {forced = true})
            else
                ai.explore(scout.id)
            end
            mem.sea_scout_t = view.time - 100000   -- again in 20 s
            mem.sea_scouts = (mem.sea_scouts or 0) + 1
            if mem.sea_scouts % 6 == 1 then
                print(string.format("navy sea_scout id=%d type=%s to=%s,%s", scout.id, scout.type, tostring(gx), tostring(gy)))
            end
        end
    elseif mem.sea_scout ~= nil and #known > 0 then
        local scout = ai.unit(mem.sea_scout)
        if scout ~= nil then
            ai.release(scout.id)   -- the enemy found: the boat fishes again (the fish verb takes it back)
        end
        mem.sea_scout = nil
    end
    -- the fleet guards the dock whatever happens
    N.fleet_step(mem, {reach = 24, skip = mem.sea_scout})
    -- a landing under way
    if mem.landing ~= nil then
        local L = mem.landing
        local result, detail = N.landing_step(L)
        if result == "landed" then
            print(string.format("landing ashore n=%d at=%d,%d retries=%d", #detail, L.site_x, L.site_y, L.retries or 0))
            local g = G.create(memory, {task = "attack", role = "landed", x = L.tx, y = L.ty, target = L.target,
                                        home_x = L.site_x, home_y = L.site_y})
            G.add(memory, g, detail)
            mem.landings = (mem.landings or 0) + 1
            mem.landing = nil
            mem.landing_t = view.time
        elseif result == "aborted" then
            print(string.format("landing aborted why=%s state=%s aboard=%d", detail, L.state, L.aboard or 0))
            if #L.soldiers > 0 then
                ai.release(L.soldiers)   -- back to the force next step
            end
            -- the transports unload where they are, at the boarding shore
            if #L.ships > 0 then
                for _, id in ipairs(L.ships) do
                    ai.unload(id, L.board_x, L.board_y)
                end
            end
            mem.landing = nil
            mem.landing_t = view.time
            mem.aborted = (mem.aborted or 0) + 1
        end
        return
    end
    -- the landed group: released when a quarter is left or it has nothing left to fight
    local landed = G.role(memory, "landed")
    if landed ~= nil and (#landed.units * 4 <= (landed.started or 1) or landed.state == "arrived") then
        print(string.format("landing over units=%d started=%d state=%s", #landed.units, landed.started or 0, landed.state))
        G.disband(memory, landed)
    end
    -- a warship for the look over the water (the fishing boats as scouts were sunk) from the Stone Age with a dock
    local dock = V.buildings(view, "b_dock")
    if dock ~= nil and dock.finished > 0 and view.epoch >= 1 then
        local war = warship_class(view)
        local warships = ai.units{category = "ship", armed = true}
        if war ~= nil and #warships == 0 and war.requested + war.in_production == 0 then
            local n = 1
            for _, r in ipairs(V.resources) do
                if war.cost[r] > 0 then
                    n = math.min(n, budget.avail[r] // war.cost[r])
                end
            end
            if n > 0 then
                order({verb = "train", type = war.type, n = 1})
            end
        end
    end
    if target == nil or view.time - (mem.landing_t or 0) < 60000 then
        return
    end
    -- the dock and the transports
    if V.total(dock) == 0 then
        if V.cost(view, "b_dock") ~= nil and view.time - (mem.dock_t or 0) >= 120000 then
            order({verb = "fish", boats = S.boats[math.min(view.epoch, 2) + 1] > 0 and S.boats[math.min(view.epoch, 2) + 1] or 2, docks = 1})
            mem.dock_t = view.time
        end
        return
    end
    if dock.finished == 0 then
        return
    end
    local tr = transport_class(view)
    local transports = N.transports()
    -- the landing's size: 1.5 x what holds the beach and the soldiers known within 40 tiles behind it, from the force
    -- at home
    if force == nil or #force.units == 0 then
        return
    end
    local sea = N.dock_sea()
    local site_x, site_y, beach = N.beach({x = target.x, y = target.y}, sea)
    if site_x == nil then
        if view.time - (mem.wait_t or 0) >= 60000 then
            mem.wait_t = view.time
            print("landing no_beach")
        end
        return
    end
    local behind = enemy_at(site_x, site_y, 40)
    local own = force.strength or 0
    local per = math.max(own // math.max(#force.units, 1), 1)
    local need_units = math.min(math.max((math.max(beach, behind // 2) * 3 // 2 + per - 1) // per, 6), #force.units)
    local cap = 0
    for _, t in ipairs(transports) do
        cap = cap + t.cap
    end
    local want_ships = 0
    if tr ~= nil then
        local tcap = ai.class(tr.type).transport or 10
        want_ships = math.min(S.landing_transports, (need_units + tcap - 1) // tcap)
    end
    if tr ~= nil and #transports < want_ships and tr.requested + tr.in_production == 0 then
        local n = 1
        for _, r in ipairs(V.resources) do
            if tr.cost[r] > 0 then
                n = math.min(n, budget.avail[r] // tr.cost[r])
            end
        end
        if n > 0 then
            order({verb = "train", type = tr.type, n = 1})
            mem.transports_asked = (mem.transports_asked or 0) + 1
        end
    end
    -- a warship or two from the account (navy_share)
    local war = warship_class(view)
    local warships = ai.units{category = "ship", armed = true}
    if war ~= nil and #warships < 2 + view.epoch // 2 and war.requested + war.in_production == 0 then
        local n = 1
        for _, r in ipairs(V.resources) do
            if war.cost[r] > 0 then
                n = math.min(n, (budget.avail[r] * S.navy_share // 100) // war.cost[r])
            end
        end
        if n > 0 then
            order({verb = "train", type = war.type, n = 1})
        end
    end
    -- the landing: at the attack time, when the force beats the beach by 1.4
    if #transports == 0 or cap < 6 then
        return
    end
    local home_force = {}
    local fx, fy = force.cx or view.town_x, force.cy or view.town_y
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and d2(u.x, u.y, fx, fy) <= 25 * 25 then
            home_force[#home_force + 1] = id
        end
    end
    local carried = {}
    local take = math.min(cap, #home_force)
    for k = 1, take do
        carried[k] = home_force[k]
    end
    local str = ai.strength(carried)
    if #carried < 6 or str * 100 < beach * 140 then
        if view.time - (mem.wait_t or 0) >= 60000 then
            mem.wait_t = view.time
            print(string.format("landing wait carried=%d str=%d beach=%d cap=%d ships=%d", #carried, str, beach, cap, #transports))
        end
        return
    end
    if view.time < (memory.army and memory.army.next_attack or 0) then
        return
    end
    local plan, why = N.plan({x = target.x, y = target.y, player = target.player}, carried, transports, {site_x = site_x, site_y = site_y})
    if plan == nil then
        if view.time - (mem.wait_t or 0) >= 60000 then
            mem.wait_t = view.time
            print(string.format("landing no_plan why=%s", why))
        end
        return
    end
    -- the soldiers leave the force for the landing
    local keep = {}
    local set = {}
    for _, id in ipairs(carried) do
        set[id] = true
    end
    for _, id in ipairs(force.units) do
        if not set[id] then
            keep[#keep + 1] = id
        end
    end
    force.units = keep
    ai.take(carried)
    ai.take(plan.ships)
    mem.landing = plan
    memory.army.next_attack = view.time + (S.attack_interval_min + ai.random(0, S.attack_interval_max - S.attack_interval_min)) * 60000
    print(string.format("landing board n=%d ships=%d site=%d,%d board=%d,%d target=%d,%d str=%d beach=%d", #carried, #plan.ships,
        plan.site_x, plan.site_y, plan.board_x, plan.board_y, target.x, target.y, str, beach))
end

return M

-- The classic's answer to a hit on a citizen (EEComputerPlayer::DamageTaken -> EECPRMNoteDanger, EECPRMMilitia,
-- EECPRMFleeRaids in EECPResourceManager.cpp) in the micro step: the personality's militia (military.militia, with
-- militia_towers) or, against a human's raid, the danger zone the gatherers leave; the zones' flight every step.
-- The engine's own reflexes skip the units a script holds (all of this seat's citizens), so the port runs them.
-- require("classic_lua.militia").
local R = require("classic_lua.resource")

local M = {}

local MILITIA_RANGE, MILITIA_TOWER_RANGE, DEFEND_RANGE = 10, 12, 30
local ODDS_K = 1500
local MILITIA_MS = 4000
local FLEE_RANGE, DANGER_RANGE, HOME_RANGE, MAX_RAIDERS, DEFENDERS = 8, 10, 12, 2, 4

local function near(u, x, y, r)
    return R.d2(u.x, u.y, x, y) <= r * r
end

-- the soldiers of a list of seen ids: hit points and attack summed
local function soldiers_of(ids)
    local out, hp, at = {}, 0, 0
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and (u.attack or 0) > 0 and (u.hp or 0) > 0 and u.category ~= "citizen" and u.category ~= "building"
            and u.category ~= "boat" then
            out[#out + 1] = u
            hp, at = hp + u.hp, at + u.attack
        end
    end
    return out, hp, at
end

local function add_zone(memory, x, y, time)
    local rm = R.state(memory)
    rm.danger = rm.danger or {}
    rm.danger[#rm.danger + 1] = {x = x, y = y, t = time}
end

-- EECPRMMilitia: the citizens within 10 tiles and the soldiers within 30 not attacking (and the built towers within
-- 12) fight when their hit points x attack is 1.5 x the raiders' within 10 (the soldiers first, then the nearest
-- citizens as the odds need); else the citizens run to the nearest tower within 30, the town centre, or the drop site
-- farthest from the attack. Once every 4 s. The raiders' strength: every one with the manifest's sees_all
-- (ai.enemy_force), else DEVIATION (fog) the attacker's soldiers seen
local function militia(memory, victim, attacker_player, time)
    local mi = memory.militia or {}
    memory.militia = mi
    if mi.last ~= nil and time - mi.last < MILITIA_MS then
        return
    end
    mi.last = time
    local x, y = victim.x, victim.y
    local raiders, rhp, rat = soldiers_of(ai.seen{player = attacker_player, near = {x, y, MILITIA_RANGE}})
    -- (with the manifest's sees_all the odds count every raider, seen or not, as the classic's: ai.enemy_force; the
    -- fighters still go for the raiders seen)
    if memory.t.sees_all and ai.enemy_force ~= nil then
        local n, ahp, aat = ai.enemy_force(attacker_player, x, y, MILITIA_RANGE)
        if n ~= nil and n > 0 then
            rhp, rat = ahp, aat
        end
    end
    if #raiders == 0 then
        return
    end
    local citizens, soldiers = {}, {}
    local hp, at = 0, 0
    for _, id in ipairs(ai.units{category = "citizen", near = {x, y, MILITIA_RANGE}}) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and u.action ~= "build" then
            citizens[#citizens + 1] = u
            hp, at = hp + (u.hp or 0), at + (u.attack or 0)
        end
    end
    for _, id in ipairs(ai.units{category = "soldier", near = {x, y, DEFEND_RANGE}}) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and u.action ~= "attack" and (u.attack or 0) > 0 then
            soldiers[#soldiers + 1] = u
            hp, at = hp + (u.hp or 0), at + (u.attack or 0)
        end
    end
    local tower
    if memory.t.personality.militia_towers then
        local best
        for _, id in ipairs(ai.units{category = "building", built = true, near = {x, y, DEFEND_RANGE}}) do
            local u = ai.unit(id)
            if u ~= nil and (u.attack or 0) > 0 and u.type:find("tower") ~= nil then
                if near(u, x, y, MILITIA_TOWER_RANGE) then
                    hp, at = hp + (u.hp or 0), at + u.attack
                end
                local dd = R.d2(u.x, u.y, victim.x, victim.y)
                if best == nil or dd < best then
                    best, tower = dd, u
                end
            end
        end
    end
    local rm = R.state(memory)
    if hp * at * 1000 >= ODDS_K * rhp * rat then
        local all = {}
        local nhp, nat = 0, 0
        for _, s in ipairs(soldiers) do
            all[#all + 1] = s
            nhp, nat = nhp + (s.hp or 0), nat + (s.attack or 0)
        end
        table.sort(citizens, function(a, b)
            local da, db = R.d2(a.x, a.y, x, y), R.d2(b.x, b.y, x, y)
            if da ~= db then
                return da < db
            end
            return a.id < b.id
        end)
        for _, c in ipairs(citizens) do
            if nhp * nat * 1000 >= ODDS_K * rhp * rat then
                break
            end
            all[#all + 1] = c
            nhp, nat = nhp + (c.hp or 0), nat + (c.attack or 0)
            R.add_idle(rm, c.id)
        end
        print(string.format("classic_lua militia fight citizens=%d soldiers=%d raiders=%d odds=%.2f tower=%d", #all - #soldiers,
            #soldiers, #raiders, rhp * rat > 0 and hp * at / (rhp * rat) or 0, tower ~= nil and 1 or 0))
        -- each to the nearest raider (batched by target)
        local by = {}
        for _, u in ipairs(all) do
            local t, bd
            for _, r in ipairs(raiders) do
                local dd = R.d2(r.x, r.y, u.x, u.y)
                if bd == nil or dd < bd then
                    t, bd = r, dd
                end
            end
            by[t.id] = by[t.id] or {}
            by[t.id][#by[t.id] + 1] = u.id
        end
        for _, rid in ipairs(ai.keys(by)) do
            ai.attack(by[rid], rid)
        end
        -- (the soldiers sent stay in the fight: their groups give them no orders for the militia's period - in the
        -- classic the militia's goal outlasts the sub-group's next order, which the port's groups would give at once)
        mi.fight = mi.fight or {}
        for _, s in ipairs(soldiers) do
            mi.fight[s.id] = time + 20000
        end
    else
        local hx, hy
        local tx, ty = memory.town_x, memory.town_y
        if tower ~= nil and not near(tower, x, y, 3) then
            hx, hy = tower.x, tower.y
        elseif tx ~= nil and tx >= 0 and not near(victim, tx, ty, HOME_RANGE) then
            hx, hy = tx, ty
        else
            local far
            for _, d in ipairs(R.drops(false)) do
                local dd = R.d2(d.x, d.y, x, y)
                if d.built and (far == nil or dd > far) then
                    far, hx, hy = dd, d.x, d.y
                end
            end
            if far == nil or far <= MILITIA_RANGE * MILITIA_RANGE then
                hx = nil
            end
        end
        print(string.format("classic_lua militia run citizens=%d soldiers=%d raiders=%d odds=%.2f tower=%d", hx ~= nil and #citizens or 0,
            #soldiers, #raiders, rhp * rat > 0 and hp * at / (rhp * rat) or 0, tower ~= nil and 1 or 0))
        if hx ~= nil and #citizens > 0 then
            local ids = {}
            for _, c in ipairs(citizens) do
                ids[#ids + 1] = c.id
                R.add_idle(rm, c.id)
            end
            ai.move(ids, hx, hy, {forced = true})
            add_zone(memory, x, y, time)
        end
    end
end

-- EECPRMNoteDanger against a human's raid (no militia): a harassing raid of at most 2 soldiers away from home makes a
-- danger zone, and up to 4 of the nearest soldiers within 30 not attacking go for the raider
local function note_danger(memory, victim, attacker, time)
    local a = ai.unit(attacker)
    if a == nil then
        return
    end
    local n = 0
    for _, id in ipairs(ai.seen{player = a.player, armed = true, near = {victim.x, victim.y, DANGER_RANGE}}) do
        local u = ai.unit(id)
        if u ~= nil and u.category ~= "citizen" and u.category ~= "boat" and u.category ~= "building" then
            n = n + 1
            if n > MAX_RAIDERS then
                return
            end
        end
    end
    local tx, ty = memory.town_x, memory.town_y
    if tx ~= nil and tx >= 0 and R.d2(victim.x, victim.y, tx, ty) < HOME_RANGE * HOME_RANGE then
        return
    end
    local new = not R.danger_at(memory, victim.x, victim.y)
    add_zone(memory, victim.x, victim.y, time)
    if not new then
        return
    end
    local list = {}
    for _, id in ipairs(ai.units{category = "soldier", near = {a.x, a.y, DEFEND_RANGE}}) do
        local u = ai.unit(id)
        if u ~= nil and u.action ~= "attack" and (u.attack or 0) > 0 then
            list[#list + 1] = id
            if #list >= DEFENDERS then
                break
            end
        end
    end
    if #list > 0 then
        ai.attack(list, attacker)
    end
end

-- the micro step's events: a hit on an own land citizen by another player's unit seen
function M.on_events(memory, events, time)
    local personality = memory.t ~= nil and memory.t.personality or nil
    if personality == nil then
        return
    end
    local humans = memory.humans or {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil then
            local v = ai.unit(e.unit)
            local a = ai.unit(e.attacker)
            if v ~= nil and a ~= nil and v.category == "citizen" and a.player ~= nil and a.player > 0
                and a.player ~= memory.me and not (memory.allies or {})[a.player] then
                if personality.militia then
                    militia(memory, v, a.player, time)
                elseif humans[a.player] then
                    note_danger(memory, v, e.attacker, time)
                end
            end
        end
    end
end

-- EECPRMFleeRaids: the gatherers within 8 tiles of a danger zone (30 s) go home and onto the idle list
function M.flee(view, memory)
    local rm = R.state(memory)
    if rm.danger == nil or #rm.danger == 0 or view.town_x == nil or view.town_x < 0 then
        return
    end
    local run = {}
    for _, r in ipairs(R.RES) do
        for _, id in ipairs(rm.lists[r]) do
            local u = ai.unit(id)
            if u ~= nil and u.category == "citizen" and u.inside == nil then
                for _, z in ipairs(rm.danger) do
                    if R.d2(u.x, u.y, z.x, z.y) <= FLEE_RANGE * FLEE_RANGE then
                        run[#run + 1] = id
                        break
                    end
                end
            end
        end
    end
    if #run > 0 then
        for _, id in ipairs(run) do
            R.add_idle(rm, id)
        end
        ai.move(run, view.town_x, view.town_y, {forced = true})
    end
end

return M

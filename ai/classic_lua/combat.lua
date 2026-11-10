-- The classic's army, first stage (PARTLY; ORIGIN in this folder): EECPCombatManager::Update's attack on its timer
-- (the target player, the group's size by EECPUnitSelector::ComputeGroupSize, the peace rules of the personality),
-- the town's defence as one defend group of the free soldiers (EECPDefenseCommander's own-town aid, simplified), the
-- groups run by lib.groups (forming, legs, targets, retreat: the sub-groups' state machines are not ported).
-- require("classic_lua.combat").
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local U = require("classic_lua.units")
local G = require("lib.groups")

local M = {}

local DEFEND_RADIUS = 34   -- (MilitaryAidPackage::Deploy: the groups within squared distance 1200 of the town hull answer its threats)

local function army(memory)
    memory.army = memory.army or {}
    return memory.army
end

-- the enemy players alive, humans and computers apart
local function enemies()
    local humans, computers = {}, {}
    for _, p in ipairs(ai.players()) do
        if p.enemy and not p.me and p.alive then
            if p.human then
                humans[#humans + 1] = p
            else
                computers[#computers + 1] = p
            end
        end
    end
    return humans, computers
end

-- what the player knows of an enemy (EECPIntelligencePlayerFile from ai.memory: DEVIATION, fog-honest): its
-- military and gatherers remembered, its building nearest the town
local function file_of(view, player)
    local tx, ty = R.town(view)
    local military, gatherers, best, bd = 0, 0, nil, nil
    for _, m in ipairs(ai.memory{player = player}) do
        if m.building then
            if tx ~= nil then
                local d = R.d2(m.x, m.y, tx, ty)
                if bd == nil or d < bd then
                    best, bd = m, d
                end
            end
        else
            local c = R.class_of(m.type)
            if c.category == "citizen" then
                gatherers = gatherers + 1
            elseif (c.attack or 0) > 0 then
                military = military + 1
            end
        end
    end
    local town
    for _, e in ipairs(view.enemies or {}) do
        if e.player == player and (e.town_x or -1) >= 0 then
            town = {x = e.town_x, y = e.town_y}
        end
    end
    return {player = player, military = military, gatherers = gatherers, near = best, distance = bd, town = town}
end

-- the attack group's size (ComputeGroupSize): a random share (attack_group_size min..max percent) of the target's
-- strength (military x the military weight + gatherers x the worker weight) x the difficulty's group modifier, 1 at least
local function group_size(memory, f)
    local b = memory.t.behavior
    local strength = (f.military * b.military_count_weight_k + f.gatherers * b.worker_count_weight_k) // 1000
    local share = T.random(b.attack_group_size_min, b.attack_group_size_max)
    local mod = T.by_difficulty(memory, b.attack_group_modifier_easy, b.attack_group_modifier_medium,
        b.attack_group_modifier_hard)
    local n = share * strength * mod // 10000
    if n <= 0 then
        n = 1
    end
    return n
end

-- the attack (CombatManager::Update when IsAttackDue): the target player (computers with the aggression's chance
-- when humans are there too), the nearest known (DEVIATION: the front overlap of the C++ is omniscient), a group of
-- the free soldiers (the defence's first) sent at its nearest known building or town; the next attack time
local function attack(view, memory)
    local cp = memory.cp
    if R.town(view) == nil or view.time < cp.next_attack then
        return
    end
    local main = require("classic_lua.main")
    local p = memory.t.personality
    local peace = (view.epoch <= 0 and not p.prehistoric_attacks) or (view.epoch == 1 and not p.stone_attacks)
    local humans, computers = enemies()
    local list = computers
    if #humans > 0 and #computers > 0 then
        if T.random_k() >= memory.t.behavior.computer_aggression_k * 10 then
            list = humans
        end
    elseif #humans > 0 then
        list = humans
    end
    local best
    for _, e in ipairs(list) do
        local f = file_of(view, e.id)
        local d = f.distance
        if d ~= nil and (best == nil or d < best.distance) then
            best = f
        elseif best == nil and f.town ~= nil then
            best = f
            best.distance = 1 << 40
        end
    end
    if best ~= nil and not peace then
        local n = group_size(memory, best)
        local x, y
        if best.near ~= nil then
            x, y = best.near.x, best.near.y
        else
            x, y = best.town.x, best.town.y
        end
        local mem = army(memory)
        local g = G.create(mem, {task = "attack", role = "attack", x = x, y = y, target = best.player})
        local home = G.role(mem, "home")
        local got = 0
        if home ~= nil then
            got = G.recruit(mem, g, n, {from = home})
        end
        if got < n then
            got = got + G.recruit(mem, g, n - got)
        end
        if got == 0 then
            G.disband(mem, g)
        end
        -- the shortfall is asked for (the unit selector's requests through Personnel: the favourite land unit)
        if got < n and cp.favorite_land ~= nil then
            U.request(view, memory, cp.favorite_land, math.min(n - got, 10))
        end
        print(string.format("classic_lua attack target=%d size=%d got=%d at=%d,%d", best.player, n, got, x, y))
    end
    main.set_next_attack(view, memory)
end

-- the soldiers in the groups (their population: one a soldier)
local function used_population(memory)
    local n = 0
    for _, g in ipairs(G.all(army(memory))) do
        n = n + #g.units
    end
    return n
end

-- GetUnitLimit: the pop cap x (1 - the citizen share); GetUnitAllowance: (defensive_unit_cap - used / limit) x limit
local function unit_allowance(view, memory)
    local b = memory.t.behavior
    local limit = view.population_cap * (1000 - b.citizen_percentage_k) // 1000
    if limit <= 0 then
        return 0
    end
    local used = used_population(memory)
    local frac_k = used * 1000 // limit
    if frac_k >= b.defensive_unit_cap_k then
        return 0
    end
    return (b.defensive_unit_cap_k - frac_k) * limit // 1000
end

-- PlanAid (every defensive_unit_update by the difficulty; the first after the replenish delay): with units allowed,
-- by the town-guard chance aid to the own town (the home group is there: nothing new), else CreateDefendGroup: a
-- defence group of a random share (defense_group_size min..max percent) of a random enemy's strength, from the free
-- soldiers and the favourite land unit requested for the rest. DEVIATION: the classic's defend groups guard choke
-- points, forests, hills and towns (AssignSubGroupTask); the port's join the home group
-- a defence group's size (ComputeGroupSize for kinds 1/5): a random share (defense_group_size min..max percent) of a
-- random enemy's strength (from what is known of it), 1 at least
local function defense_size(view, memory)
    local b = memory.t.behavior
    local humans, computers = enemies()
    local all = {}
    for _, e in ipairs(humans) do
        all[#all + 1] = e
    end
    for _, e in ipairs(computers) do
        all[#all + 1] = e
    end
    if #all == 0 then
        return 0
    end
    local e = all[ai.random(1, #all)]
    local f = file_of(view, e.id)
    local strength = (f.military * b.military_count_weight_k + f.gatherers * b.worker_count_weight_k) // 1000
    local n = T.random(b.defense_group_size_min, b.defense_group_size_max) * strength // 100
    return n > 0 and n or 1
end

-- the soldiers for a defence group: the free ones join the home group, the rest are requested (the favourite land
-- unit: the unit selector's favourites through Personnel's requests)
local function staff_defense(view, memory, n, why)
    local cp = memory.cp
    local mem = army(memory)
    local home = G.role(mem, "home")
    local free = G.free_soldiers(view.town_x, view.town_y)
    local got = math.min(#free, n)
    if home ~= nil and got > 0 then
        local ids = {}
        for k = 1, got do
            ids[k] = free[k]
        end
        G.add(mem, home, ids)
    end
    if got < n and cp.favorite_land ~= nil then
        U.request(view, memory, cp.favorite_land, n - got)
    end
    print(string.format("classic_lua defend.%s size=%d free=%d", why, n, got))
end

-- AidOwnTowns: the own town without an aid package gets one; the package's first gathering counter-attacks the
-- threats (CounterAttack: a group of special and counter units) - the port staffs one defence group then; the
-- package's later gatherings (every 20-35 s while the town's threat is over aid_threat_threshold) the same.
-- DEVIATION: the town threat is the strength of the enemy soldiers seen within 30 tiles over the own army's
local function aid_own_town(view, memory)
    local cp = memory.cp
    local b = memory.t.behavior
    if cp.package_next ~= nil and view.time < cp.package_next then
        return
    end
    local tx, ty = R.town(view)
    if tx == nil then
        return
    end
    if cp.package_next == nil then
        cp.package_next = view.time + 35000
        staff_defense(view, memory, defense_size(view, memory), "package")
        return
    end
    -- the threat: the enemies seen near over the own strength there
    local ehp = 0
    for _, id in ipairs(ai.seen{enemy = true, armed = true, near = {tx, ty, 30}}) do
        local u = ai.unit(id)
        if u ~= nil and u.category ~= "citizen" and u.category ~= "building" then
            ehp = ehp + (u.hp or 0) * (u.attack or 0)
        end
    end
    local own = 0
    for _, id in ipairs(ai.units{category = "soldier", near = {tx, ty, 30}}) do
        local u = ai.unit(id)
        if u ~= nil then
            own = own + (u.hp or 0) * (u.attack or 0)
        end
    end
    local threat_k = ehp > 0 and ehp * 1000 // (ehp + own) or 0
    if threat_k > b.aid_threat_threshold_k then
        cp.package_next = view.time + 20000
        if unit_allowance(view, memory) > 0 then
            staff_defense(view, memory, defense_size(view, memory), "threat")
        end
    else
        cp.package_next = view.time + 35000
    end
end

local function plan_aid(view, memory)
    local cp = memory.cp
    local b = memory.t.behavior
    if cp.next_aid == nil then
        cp.next_aid = view.time + T.random(b.replenish_delay_min, b.replenish_delay_max) * 60000
    end
    if not cp.defense_enabled or view.time < cp.next_aid then
        return
    end
    local allowance = unit_allowance(view, memory)
    if allowance > 0 and T.random_k() >= b.town_guard_percentage_k then
        local n = math.min(defense_size(view, memory), allowance)
        if n > 0 then
            staff_defense(view, memory, n, "group")
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

-- the town's defence (EECPDefenseCommander's aid of the own town, simplified): every 8 s with defence enabled, one
-- defend group at the town centre takes the free soldiers
local function defend(view, memory)
    local cp = memory.cp
    if cp.next_defense ~= nil and view.time < cp.next_defense then
        return
    end
    cp.next_defense = view.time + 8000
    local tx, ty = R.town(view)
    if tx == nil or not cp.defense_enabled then
        return
    end
    local mem = army(memory)
    local home = G.role(mem, "home")
    if home == nil then
        home = G.create(mem, {task = "defend", role = "home", x = tx, y = ty, radius = DEFEND_RADIUS})
    end
    home.x, home.y = tx, ty
    local free = G.free_soldiers(tx, ty)
    if #free > 0 then
        G.add(mem, home, free)
    end
end

function M.step(view, memory)
    local mem = army(memory)
    G.init(mem)
    defend(view, memory)
    if memory.cp.defense_enabled and view.time >= (memory.me + 5) * 1000 then
        aid_own_town(view, memory)   -- (the defence commander's first update at the player id + 5 s)
    end
    plan_aid(view, memory)
    attack(view, memory)
    -- the attack groups done (arrived with nothing left, or emptied) come home into the defence
    for _, g in ipairs(G.all(mem)) do
        if g.role == "attack" and (#g.units == 0 or g.state == "done") then
            local home = G.role(mem, "home")
            if home ~= nil and #g.units > 0 then
                G.add(mem, home, g.units)
            end
            if #g.units == 0 or home ~= nil then
                mem.groups[g.id] = nil
            else
                G.disband(mem, g)
            end
        end
    end
    G.step(view, mem)
end

return M

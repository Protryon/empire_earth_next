-- Age of AI port, GOLD AND IRON (upstream dll/ai/metal.c, docs/economy.md "Gold and iron"; army.c MIX for
-- workers_to_metals; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.metal").
--
-- The playtester: "convert the economy into attack and defence". At a high population the army is limited by gold
-- and iron, not citizens, food or wood: every soldier costs one of them. metal_plan ([ai_base], bits; empty = off
-- upstream: measured as a gain on the large island maps without the cheat and a loss elsewhere), every bit only from
-- a population cap of metal_cap (250; the sim's Small maps have 400):
--   1 best_per_metal (VALUE): a land soldier the army asks for (here: army.lua's strongest affordable, pick_soldier)
--     is trained as the one that does most against the enemy army it knows per gold + iron, when that one is 25%
--     better: the value of a type c is the enemy soldiers one c kills in its life against the enemy's mix (the
--     memory: its soldiers seen within 10 minutes, by type; fewer than 3 known: its own trainable types as the mix) -
--     c's damage per second at each over that one's hit points, times c's hit points over the mix's damage per second
--     at c (a ranged c behind the line takes 60% of a melee mix's); a type that is already half the army counts a
--     little less (1 / (1 + share)). Its price: gold + iron + a quarter of food and wood + half the stone.
--   2 no_waiting (NOWAIT): not applicable - the port never saves for a refused unit (substitute_wait: army.lua
--     trains the strongest it can pay at once), so there is no wait to cut short.
--   4 surplus_cheap (SPARE): VALUE's price weighs food, wood and stone by what lies unused: 1500+ beyond the reserve
--     x0.05, under twice the unit's price of it x1, else as above.
--   8 fleet_by_enemy (FLEET): the fleet's size target (navy.lua, navy_fleet_pop of the cap x the strength) is at most
--     metal_fleet_pct (250%) of the enemy warships it knows (seen within 10 minutes) + 6, at least metal_fleet_min
--     (20) x the map strength.
--   16 calm_navy_cap (CALM): on a water map, the sea calm for 3 minutes (towns.lua sea_held: no enemy warship seen in
--     its waters for 5 minutes, its warships at least the enemy's known) - the navy account keeps at most 100 gold
--     and 100 iron; the rest is the army's.
-- workers_to_metals (army.c MIX, army_plan's bit 8, off upstream): after the worker targets, while food lies unused
-- (the 2-minute low of food above the reserve at least army_mix_food, 500) and the army's orders were refused for
-- wood, gold or iron, up to 8 workers of the food target go to the most refused of them with room (wood always, a
-- metal up to its mine places near drop sites) - food keeps two thirds of its target and its floor.
-- DEVIATIONS (the engine): the damage of a shot is the attack (stats.lua: no damage-class or hit-factor tables, no
-- armour are exposed; the anti-air rule by name); the enemy's stats are the reader's own class data (ai.class has no
-- player argument); a type's line (inf / rng / cav / sie / oth, the log's) from its producer in view.costs (ai.class
-- has no family field) and the siege, ships and aircraft by class-name patterns (lib fleet.lua F.kind); the army's
-- refusals are counted once per 10-s training look (upstream's game asked many times a minute: the threshold is 5
-- in two minutes for upstream's 10).
local V = require("lib.view")
local S = require("ageofai.settings")
local M = {}

local MT_CAND = 48
local MIX_SHORT = 5           -- DEVIATION (upstream 10: refusals by request; the port looks once in 10 s)
local SHIP_PRODUCERS = {b_dock = true, b_navy_yard = true, b_space_dock = true}

local function has(bits, bit)
    return (bits & bit) ~= 0
end

-- the bits in force: metal_plan from a population cap of metal_cap
function M.plan(view)
    local bits = S.metal_plan or 0
    if bits == 0 or view.population_cap < (S.metal_cap or 250) then
        return 0
    end
    return bits
end

local function kind(t)
    local F = require("ageofai.fleet")
    return F.kind(t)
end

local function is_aa(t)
    t = t or ""
    return string.sub(t, 1, 4) == "b_aa" or string.sub(t, 1, 2) == "aa" or string.find(t, "_aa_", 1, true) ~= nil
        or string.find(t, "missile_trooper", 1, true) ~= nil or string.find(t, "anti_missile", 1, true) ~= nil
end

-- econ.c land_soldier: a land unit that is no siege, ram, citizen, priest or prophet (heroes count)
function M.land_soldier(c)
    if c == nil or c.building or not c.military or c.producer == nil or SHIP_PRODUCERS[c.producer] then
        return false
    end
    local t = c.type
    if t == "citizen" or string.find(t, "priest", 1, true) or string.find(t, "prophet", 1, true) then
        return false
    end
    local k = kind(t)
    return k ~= "ship" and k ~= "air" and k ~= "siege" and k ~= "boat" and k ~= "transport"
end

-- the line of an own type for the log (mt_line): by its producer
function M.line(view, t)
    local c = V.cost(view, t)
    if c == nil or c.building then
        return "oth"
    end
    if kind(t) == "siege" then
        return "sie"
    end
    if c.producer == "b_barracks" then
        return "inf"
    elseif c.producer == "b_archery_range" then
        return "rng"
    elseif c.producer == "b_stable" then
        return "cav"
    end
    return "oth"
end

-- damage per second of type a at type t (mt_dps: stats.c's shot over reload + 2 s; here the attack, the AA rule)
local function dps(a, ca, t, ct)
    local att = ca.attack or 0
    if att <= 0 or is_aa(a) then   -- the mix has no aircraft: anti-air hits nothing in it
        return 0
    end
    local reload = (ca.reload or 0) > 0 and (ca.reload / 1000.0) or 1.0
    return math.max(math.floor(att + 0.5), 1) / (reload + 2.0)
end

-- the values of the trainable land soldiers against the known enemy mix, every 30 s (mt_values); stored x 1000
function M.values(view, memory)
    memory.metal = memory.metal or {}
    local mm = memory.metal
    if mm.val_t ~= nil and view.time >= mm.val_t and view.time - mm.val_t < 30000 then
        return mm
    end
    mm.val_t = view.time
    local cand = {}
    for _, c in ipairs(view.costs) do
        if #cand < MT_CAND and M.land_soldier(c) and V.sum(c.cost) >= 20 then
            local cls = ai.class(c.type)
            if cls ~= nil and (cls.attack or 0) > 0 and (cls.hp or 0) > 0 then
                cand[#cand + 1] = c.type
            end
        end
    end
    -- the enemy's mix: type -> count (the owners share the reader's class data here)
    local mix, order, tot = {}, {}, 0
    for _, r in ipairs(ai.memory{since = math.max(view.time - 600000, 0)}) do
        if not r.building and #order <= 32 then
            local k = kind(r.type)
            if k ~= "ship" and k ~= "air" and k ~= "boat" and k ~= "transport" then
                local cls = ai.class(r.type)
                if cls ~= nil and (cls.attack or 0) > 0 and (cls.hp or 0) > 0 and r.type ~= "citizen" then
                    if mix[r.type] == nil then
                        if #order < 32 then
                            mix[r.type] = 0
                            order[#order + 1] = r.type
                        end
                    end
                    if mix[r.type] ~= nil then
                        mix[r.type] = mix[r.type] + 1
                        tot = tot + 1
                    end
                end
            end
        end
    end
    mm.foe = tot
    if tot < 3 then   -- nothing known: the mirror (its own trainable types, one each)
        mix, order = {}, {}
        for _, t in ipairs(cand) do
            mix[t] = 1
            order[#order + 1] = t
        end
    end
    local val = {}
    for _, c in ipairs(cand) do
        local cc = ai.class(c)
        local chp, crange = cc.hp, (cc.range or 0) / 100.0
        local kill, inc, w = 0.0, 0.0, 0.0
        for _, e in ipairs(order) do
            local ec = ai.class(e)
            local n = mix[e]
            if ec ~= nil and (ec.hp or 0) > 0 then
                local erange = (ec.range or 0) / 100.0
                kill = kill + n * dps(c, cc, e, ec) / ec.hp
                local f = (crange >= 3.0 and erange < 1.5) and 0.6 or 1.0   -- a ranged one behind the line, a melee mix
                inc = inc + n * f * dps(e, ec, c, cc)
                w = w + n
            end
        end
        local v = 0.0
        if w > 0 and kill > 0 then
            v = (kill / w) * chp / (inc > 0.01 and inc / w or 0.01)
        end
        val[c] = math.floor(v * 1000 + 0.5)
    end
    mm.val = val
    mm.cand = cand
    mm.mix_n = #order
    return mm
end

-- the own land soldiers by type (VALUE's share), every 30 s
local function own_types(view, memory)
    local mm = memory.metal
    if mm.own_t ~= nil and view.time >= mm.own_t and view.time - mm.own_t < 30000 then
        return
    end
    mm.own_t = view.time
    local own, n = {}, 0
    local ids = ai.units{category = "soldier"}
    if #ids > 0 then
        local cols = ai.get(ids, {"type"})
        for k = 1, #ids do
            local t = cols.type[k]
            if t ~= nil and kind(t) ~= "siege" then
                own[t] = (own[t] or 0) + 1
                n = n + 1
            end
        end
    end
    mm.own, mm.army = own, n
end

-- VALUE's price in hundredths (SPARE: food, wood and stone by what lies unused)
function M.price(view, c, stock, reserve, bits)
    local p = 0
    for _, r in ipairs(V.resources) do
        local cost = c.cost[r]
        if cost > 0 then
            local w = (r == "gold" or r == "iron") and 100 or r == "stone" and 50 or 25
            if (r == "food" or r == "wood" or r == "stone") and has(bits, 4) and stock ~= nil and reserve ~= nil then
                local spare = stock[r] - (reserve[r] or 0)
                w = spare >= 1500 and 5 or spare < 2 * cost and 100 or w
            end
            p = p + w * cost
        end
    end
    return p > 100 and p or 100
end

local function value_of(view, memory, c, stock, reserve, bits)
    local mm = memory.metal
    local v = mm.val[c.type]
    if v == nil then
        return 0.0
    end
    local share = 0.0
    if (mm.army or 0) > 10 then
        share = ((mm.own or {})[c.type] or 0) / mm.army
    end
    return (v / 1000.0) / (1.0 + share) / (M.price(view, c, stock, reserve, bits) / 100.0)
end

-- VALUE (econ.c train_budget, a land soldier's request): the type to train instead of asked (nil: as asked) and how
-- many of it vis pays. asked: the cost entry army.lua picked (nil: nothing payable); want: how many it wants
function M.choose(view, memory, asked, vis, stock, reserve, want)
    local bits = M.plan(view)
    if not has(bits, 1) or view.epoch < 1 then
        return nil
    end
    M.values(view, memory)
    own_types(view, memory)
    local mm = memory.metal
    local va = asked ~= nil and value_of(view, memory, asked, stock, reserve, bits) or 0.0
    local best, bn, bv = nil, 0, 0.0
    for _, t in ipairs(mm.cand) do
        local c = V.cost(view, t)
        if c ~= nil and c.producers > 0 then
            local n, paid = want, false
            for _, r in ipairs(V.resources) do
                if c.cost[r] > 0 then
                    paid = true
                    n = math.min(n, (vis[r] or 0) // c.cost[r])
                end
            end
            if paid and n > 0 then
                local s = value_of(view, memory, c, stock, reserve, bits)
                if s > bv then
                    best, bn, bv = c, n, s
                end
            end
        end
    end
    if best == nil or (asked ~= nil and best.type == asked.type) or (va > 0 and bv < 1.25 * va) then
        if va > 0 then
            mm.kept = (mm.kept or 0) + 1
        end
        return nil
    end
    mm.picks = (mm.picks or 0) + 1
    if view.time - (mm.pick_log_t or -100000) >= 60000 then
        mm.pick_log_t = view.time
        print(string.format("metal pick asked=%s va=%d got=%s bv=%d n=%d foe=%d picks=%d kept=%d",
            asked ~= nil and asked.type or "-", math.floor(va * 1000 + 0.5), best.type, math.floor(bv * 1000 + 0.5), bn,
            mm.foe or 0, mm.picks or 0, mm.kept or 0))
    end
    return best, bn
end

-- the army's refusals by resource (army.c am_st short_: the budget's refused orders in the last minute and the one
-- before), noted by army.lua once per training look when nothing is payable
function M.note_short(view, memory, cost, avail)
    memory.metal = memory.metal or {}
    local mm = memory.metal
    mm.short_cur = mm.short_cur or {}
    mm.short_last = mm.short_last or {}
    if view.time - (mm.short_t or 0) >= 60000 then
        mm.short_t = view.time
        mm.short_last = mm.short_cur
        mm.short_cur = {}
    end
    if cost == nil then
        return
    end
    for _, r in ipairs(V.resources) do
        if cost[r] > 0 and (avail[r] or 0) < cost[r] then
            mm.short_cur[r] = (mm.short_cur[r] or 0) + 1
        end
    end
end

-- MIX (army.c ap_targets): workers from piled-up food to the wood, gold or iron the army lacks. target is econ.lua's
-- worker plan (changed in place); feed its food floor; places the mines' places near drop sites; reserve the epoch's
function M.workers(view, memory, target, feed, places, reserve)
    if not S.workers_to_metals or view.epoch < 2 or target == nil then
        return
    end
    memory.metal = memory.metal or {}
    local mm = memory.metal
    -- FLOAT's samples: the food above the reserve every 10 s, 12 kept
    mm.fl = mm.fl or {}
    local fl = mm.fl
    fl[#fl + 1] = view.stock.food - (reserve.food or 0)
    while #fl > 12 do
        table.remove(fl, 1)
    end
    if #fl < 12 then
        return
    end
    local mn = fl[1]
    for k = 2, 12 do
        mn = math.min(mn, fl[k])
    end
    if mn < (S.army_mix_food or 500) then
        return
    end
    local floor_ = math.max(target.food * 2 // 3, feed or 0)
    local moves = math.min(target.food - floor_, 8)
    local done = {}
    local short_cur, short_last = mm.short_cur or {}, mm.short_last or {}
    while moves > 0 do
        local best, bs = nil, MIX_SHORT - 1
        for _, r in ipairs({"wood", "gold", "iron"}) do
            local sh = (short_cur[r] or 0) + (short_last[r] or 0)
            if not done[r] and sh > bs then
                best, bs = r, sh
            end
        end
        if best == nil then
            return
        end
        done[best] = true
        local lim = best == "wood" and 1000000 or math.max((places or {})[best] or 0, view.gatherers[best])
        while moves > 0 and target[best] < lim do
            target.food = target.food - 1
            target[best] = target[best] + 1
            moves = moves - 1
            mm.mixed = (mm.mixed or 0) + 1
        end
    end
end

-- the enemy warships known (seen within 10 minutes; fleet.lua's roles: warships and submarines), every 30 s
local function enemy_warships(view, memory)
    local mm = memory.metal
    if mm.war_t ~= nil and view.time >= mm.war_t and view.time - mm.war_t < 30000 then
        return mm.war_n or 0
    end
    mm.war_t = view.time
    local F = require("ageofai.fleet")
    local n = 0
    for _, r in ipairs(ai.memory{since = math.max(view.time - 600000, 0)}) do
        if not r.building and F.kind(r.type) == "ship" and F.role(r.type) ~= 0 then
            n = n + 1
        end
    end
    mm.war_n = n
    return n
end

-- FLEET (navy.c navy_tick, the stage-4 target t): at most what the sea needs. strength: the map strength in percent
function M.fleet_target(view, memory, t, strength)
    if not has(M.plan(view), 8) or t <= 0 then
        return t
    end
    memory.metal = memory.metal or {}
    local n = enemy_warships(view, memory)
    local need = (n * (S.metal_fleet_pct or 250) + 99) // 100 + 6
    local floor_ = ((S.metal_fleet_min or 20) * (strength or 0) + 50) // 100
    if need < floor_ then
        need = floor_
    end
    if need < t then
        memory.metal.fleet_cut = (memory.metal.fleet_cut or 0) + 1
        return need
    end
    return t
end

-- CALM: the navy account's gold and iron while the sea is calm (nmem: navy.lua's memory, acc in hundredths)
function M.calm(view, memory, nmem, navy_on)
    memory.metal = memory.metal or {}
    local mm = memory.metal
    if not has(M.plan(view), 16) or not navy_on then
        mm.calm_since = 0
        return
    end
    local TW = require("ageofai.towns")
    if not TW.sea_held(view, view.time) then
        mm.calm_since = 0
        return
    end
    if (mm.calm_since or 0) == 0 then
        mm.calm_since = view.time > 0 and view.time or 1
        return
    end
    if view.time - mm.calm_since < 180000 then
        return
    end
    for _, r in ipairs({"gold", "iron"}) do
        if (nmem.acc[r] or 0) > 100 * 100 then
            mm.navy_freed = (mm.navy_freed or 0) + (nmem.acc[r] - 100 * 100) // 100
            nmem.acc[r] = 100 * 100
            mm.navy_cut = (mm.navy_cut or 0) + 1
        end
    end
end

-- every step: the 5-minute line (mt_log, the autotest's parts the port has: the stock's gold + iron, the army and
-- the known enemy by line, what the plan did)
function M.tick(view, memory)
    memory.metal = memory.metal or {}
    local mm = memory.metal
    if M.plan(view) == 0 then
        return
    end
    if mm.log_t ~= nil and view.time >= mm.log_t and view.time - mm.log_t < 300000 then
        return
    end
    mm.log_t = view.time
    own_types(view, memory)
    local army = {inf = 0, rng = 0, cav = 0, sie = 0, oth = 0}
    for _, t in ipairs(ai.keys(mm.own or {})) do
        local l = M.line(view, t)
        army[l] = army[l] + mm.own[t]
    end
    print(string.format("metal plan=%d min=%d gi=%d army inf=%d rng=%d cav=%d oth=%d foe=%d types=%d picks=%d kept=%d navy_cut=%d freed=%d fleet_cut=%d mixed=%d",
        M.plan(view), view.time // 60000, view.stock.gold + view.stock.iron, army.inf, army.rng, army.cav, army.oth,
        mm.foe or 0, mm.mix_n or 0, mm.picks or 0, mm.kept or 0, mm.navy_cut or 0, mm.navy_freed or 0, mm.fleet_cut or 0,
        mm.mixed or 0))
end

return M

-- Age of AI port, the tactics (upstream dll/ai/attack.c the siege controller, garrison.c the settlement upgrades,
-- counterfire.c; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.tactics").
--
-- SIEGE (attack_siege): the AI keeps 2 + a quarter of the known enemy defences siege weapons, at most 4, from the
-- Bronze Age (a Siege Factory first); 3 lost within 10 minutes: no new ones for 10 minutes. They gather at home and
-- go with the force (lib.groups: a member like any other, but with the force's attack-move the engine keeps them at
-- their range).
-- SETTLEMENT UPGRADES (garrison_upgrades): every 20 s the drop site with the most metal mines within reach (their
-- gatherers counting) that has room - a Settlement before the rest (5 make a Town Center, 15 a Capitol) - takes up to
-- 10 spare citizens of its land, the nearest first: idle ones, then wood cutters while the wood stock is over 5000;
-- a holder ordered waits a minute. Each citizen inside raises the stone, gold and iron dropped there.
-- COUNTER-FIRE (counter_fire): every 2 s (the micro step) the enemy shooters that hit own units from beyond their
-- reach (the attacked events: the attacker further off than the victim's range + 1, its own range 5 tiles at
-- least); shooters within 6 tiles of each other are one target; the own soldiers within 24 tiles, nearest first,
-- join until they are 1.5 x the enemy fighters seen within 10 tiles of the shooter (at most 60); short of that nobody
-- goes. They are committed for 45 s (the force's orders leave them alone).
local V = require("lib.view")
local S = require("ageofai.settings")
local AT = require("ageofai.attack")   -- [attack.c] the siege's target and firing position
local LA = require("lib.army")        -- [forces.c] goal_near
local G = require("lib.groups")       -- [bunker.c] the reserve takes the force's soldiers at home
local M = {}

local SIEGE = {"gun_siege13_paladin_cannon", "gun_siege10_howitzer_cannon", "gun_siege09_serpentine_cannon",
               "gun_siege07_basilisk_cannon", "gun_siege06_trebuchet", "gun_siege04_catapult"}
local SIEGE_SET = {}
for _, t in ipairs(SIEGE) do
    SIEGE_SET[t] = true
end
M.SIEGE_SET = SIEGE_SET
local TOWNS = {b_settlement = 1, b_town_center = 2, b_capitol = 3}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- ---------------------------------------------------------------------------------------------------------------
-- Siege

function M.siege(view, mem, order, budget)
    if not S.attack_siege or view.epoch < 3 or view.time - (mem.siege_t or 0) < 30000 then
        return
    end
    mem.siege_t = view.time
    -- the known enemy defences (armed buildings in the memory)
    local def = 0
    for _, m in ipairs(ai.memory{buildings = true}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            def = def + 1
        end
    end
    local want = def > 0 and math.min(2 + def // 4, 4) or 0
    local have = 0
    for _, t in ipairs(SIEGE) do
        local ids = ai.units{type = t}
        have = have + #ids
    end
    -- losses: 3 within 10 minutes pause the training for 10
    if (mem.siege_had or 0) > have then
        mem.siege_losses = mem.siege_losses or {}
        local l = mem.siege_losses
        l[#l + 1] = view.time
        while #l > 3 do
            table.remove(l, 1)
        end
        if #l == 3 and view.time - l[1] < 600000 then
            mem.siege_pause = view.time + 600000
            print(string.format("siege pause lost=3 within=%d", (view.time - l[1]) // 1000))
        end
    end
    mem.siege_had = have
    if have >= want or view.time < (mem.siege_pause or 0) then
        mem.special = nil   -- [priority.c] nothing wanted: the special row empty
        return
    end
    local factory = V.buildings(view, "b_siege_factory")
    if V.total(factory) == 0 then
        if V.cost(view, "b_siege_factory") ~= nil and view.time - (mem.siege_factory_t or 0) >= 300000 then
            order({verb = "build", type = "b_siege_factory", near = "town"})
            mem.siege_factory_t = view.time
        end
        return
    end
    if factory.finished == 0 then
        return
    end
    for _, t in ipairs(SIEGE) do
        local c = V.cost(view, t)
        if c ~= nil and c.producers > 0 then
            local n = want - have
            for _, r in ipairs(V.resources) do
                if c.cost[r] > 0 then
                    -- [attack.c] a priority order (g_priority_mil): the stock above the epoch reserve, not the
                    -- military account alone. [priority.c] what the list leaves a priority order (budget.order)
                    n = math.min(n, (budget.order or budget.civil)[r] // c.cost[r])
                end
            end
            if n > 0 and c.requested + c.in_production == 0 then
                order({verb = "train", type = t, n = n})
                mem.siege_trained = (mem.siege_trained or 0) + n
                mem.special = nil
                return c
            end
            -- [priority.c] the special row (econ.c special_want SP_SIEGE): what it saves for is held from the
            -- ordinary soldiers (and warships) for SP_KEEP
            if n <= 0 then
                mem.special = {type = t, n = want - have, t = view.time > 0 and view.time or 1}
            end
            return nil   -- the newest it can make: short of its price
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- [attack.c] The siege's shelling (siege_tick): every siege_fire_every while the force is out, its siege weapons
-- (the force's members of the siege types) take the target attack.lua names - the force's breach wall, else the
-- nearest known defence they outrange, within siege_fire_near of the force's middle (the force is the escort;
-- upstream's waves went from home with idle soldiers along) - from the firing position (range - 0.7 from it, out of
-- every other known defence): those within 2 tiles of it attack the target, the rest move there (a forced move);
-- held while enemy soldiers are seen within 8 tiles of the position ("enemy soldiers on the way"). They are
-- committed for three looks (the force's orders leave them alone); the force home ends it (army.lua).

function M.siege_fire(view, mem, force)
    mem.shell = mem.shell or {}
    local now = view.time
    for _, id in ipairs(ai.keys(mem.shell)) do
        if mem.shell[id] <= now then
            mem.shell[id] = nil
        end
    end
    if not S.attack_siege or force == nil or not mem.out or now - (mem.shell_t or 0) < S.siege_fire_every then
        return
    end
    mem.shell_t = now
    local siege = {}
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and SIEGE_SET[u.type] and u.inside == nil then
            siege[#siege + 1] = u
        end
    end
    if #siege == 0 then
        return
    end
    local t, px, py, srange = AT.shelling(mem, siege)
    if t == nil then
        return
    end
    local cx, cy = force.cx or force.x, force.cy or force.y
    if d2(t.x, t.y, cx, cy) > S.siege_fire_near * S.siege_fire_near then
        return
    end
    local foes = ai.seen{enemy = true, category = "soldier", near = {px, py, 8}, limit = 1}
    if #foes > 0 then
        mem.shell_held = (mem.shell_held or 0) + 1
        return
    end
    local fire, go = {}, {}
    for _, u in ipairs(siege) do
        if d2(u.x, u.y, px, py) <= 4 then
            if u.target ~= t.id then
                fire[#fire + 1] = u.id
            end
        elseif not (u.action == "move" and LA.goal_near(u, px, py, 2)) then   -- [forces.c] goal_near
            go[#go + 1] = u.id
        end
        mem.shell[u.id] = now + S.siege_fire_every * 3
    end
    if #fire > 0 then
        ai.attack(fire, t.id)
    end
    if #go > 0 then
        ai.move(go, px, py, {forced = true})
    end
    if t.id ~= (mem.shell_target or 0) then
        mem.shell_target = t.id
        mem.shells = (mem.shells or 0) + 1
        print(string.format("siege shell target=%s at=%d,%d from=%d,%d range=%d siege=%d", t.type, t.x, t.y, px, py,
            srange // 100, #siege))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Settlement upgrades

-- the metal mines within the bonus's reach of a holder, each once and once more per gatherer
local function mines_near(u)
    local score = 0
    local res = ai.resources{near = {u.x + 1, u.y + 1, 5}}
    for _, id in ipairs(res) do
        local r = ai.unit(id)
        if r ~= nil and (r.kind == "stone" or r.kind == "gold" or r.kind == "iron") and (r.amount or 0) > 0
                and d2(r.x, r.y, u.x + 1, u.y + 1) <= 20 then
            score = score + 1 + (r.workers or 0)
        end
    end
    return score
end

function M.garrison(view, mem, order, wanted)
    if not S.garrison_upgrades or view.time - (mem.gu_t or 0) < S.gu_every then
        return
    end
    mem.gu_t = view.time
    -- spare citizens only (the player: "as long as it only pulls from excess"): the town at its citizens wanted
    -- with idle ones, or the wood piled up; never under 30 citizens (on an island the citizens waiting for a drop
    -- site are idle too: a first draft put the whole workforce into the settlements)
    local spare = view.citizens >= 30 and ((view.idle >= 4 and view.citizens >= (wanted or 0) - 2) or view.stock.wood > S.gu_wood)
    if not spare then
        return
    end
    mem.gu_sent = mem.gu_sent or {}
    local best, best_score, best_kind = nil, 0, 0
    local holders = ai.units{category = "building", built = true}
    for _, id in ipairs(holders) do
        local u = ai.unit(id)
        local kind = u ~= nil and TOWNS[u.type] or nil
        if kind ~= nil then
            local c = ai.class(u.type)
            local room = (c and c.garrison or 0) - (u.garrisoned or 0)
            local key = u.x * 4096 + u.y
            if room > 0 and view.time - (mem.gu_sent[key] or -100000) >= S.gu_wait then
                local score = mines_near(u)
                local raiders = ai.seen{enemy = true, armed = true, category = "soldier", near = {u.x, u.y, 12}, limit = 1}
                if score > 0 and #raiders == 0
                        and (best == nil or (kind == 1 and best_kind ~= 1) or ((kind == 1) == (best_kind == 1) and score > best_score)) then
                    best, best_score, best_kind = u, score, kind
                end
            end
        end
    end
    if best == nil then
        return
    end
    local c = ai.class(best.type)
    local want = math.min((c and c.garrison or 0) - (best.garrisoned or 0), S.gu_batch)
    -- the spare citizens of its land: idle ones, then wood cutters while the wood is piled up
    local land = ai.tile(best.x, best.y)
    land = land and land.continent or 0
    local picks = {}
    local idle = ai.units{category = "citizen", idle = true, near = {best.x, best.y, 60}}
    for _, id in ipairs(idle) do
        if #picks >= want then
            break
        end
        local u = ai.unit(id)
        if u ~= nil and u.held == "engine" and u.inside == nil then
            local t = ai.tile(u.x, u.y)
            if t ~= nil and (t.continent == land or land == 0) then
                picks[#picks + 1] = id
            end
        end
    end
    if #picks < want and view.stock.wood > S.gu_wood then
        local cutters = ai.units{category = "citizen", action = "gather", near = {best.x, best.y, 40}}
        for _, id in ipairs(cutters) do
            if #picks >= want then
                break
            end
            local u = ai.unit(id)
            if u ~= nil and u.held == "engine" and u.target ~= nil then
                local r = ai.unit(u.target)
                if r ~= nil and r.kind == "wood" then
                    picks[#picks + 1] = id
                end
            end
        end
    end
    if #picks == 0 then
        return
    end
    local code, reason, n = ai.garrison(picks, best.id)
    if code == "ok" and n > 0 then
        mem.gu_sent[best.x * 4096 + best.y] = view.time
        mem.gu_citizens = (mem.gu_citizens or 0) + n
        print(string.format("garrison n=%d into=%s at=%d,%d inside=%d mines=%d", n, best.type, best.x, best.y, best.garrisoned or 0, best_score))
        -- the citizens are the engine's again once inside (they are used up); until then they walk on their own
        ai.release(picks)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Counter-fire (the micro step)

-- events: the attacked events since the last call. Own units hit by a seen shooter from beyond their reach
function M.counterfire(mem, events, now)
    if not S.counter_fire then
        return
    end
    mem.cf = mem.cf or {}
    local cf = mem.cf
    -- the commitments still running
    for _, id in ipairs(ai.keys(cf)) do
        if cf[id] <= now then
            cf[id] = nil
        end
    end
    local shooters = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil then
            local a = ai.unit(e.attacker)
            local v = ai.unit(e.unit)
            if a ~= nil and v ~= nil and (a.range or 0) >= 500 and v.category ~= "resource" then
                local reach = ((v.range or 0) // 100 + 1)
                if d2(a.x, a.y, v.x, v.y) > reach * reach then
                    shooters[a.id] = a
                end
            end
        end
    end
    local ids = ai.keys(shooters)
    if #ids == 0 then
        return
    end
    local done = {}
    for _, id in ipairs(ids) do
        if not done[id] then
            local e = shooters[id]
            local cluster = {}
            for _, id2 in ipairs(ids) do
                if not done[id2] and d2(shooters[id2].x, shooters[id2].y, e.x, e.y) <= 36 then
                    done[id2] = true
                    cluster[#cluster + 1] = id2
                end
            end
            -- the enemy fighters seen within 10 of the shooter
            local foes = ai.seen{enemy = true, armed = true, near = {e.x, e.y, 10}}
            local foe = ai.strength(foes)
            -- own soldiers within 24, nearest first, not committed, not inside, on its land
            local own = ai.units{category = "soldier", near = {e.x, e.y, 24}}
            local land = ai.tile(e.x, e.y)
            land = land and land.continent or 0
            local cands, str, on = {}, 0, 0
            local cols = ai.get(own, {"hp", "attack", "inside", "x", "y", "target"})
            for k = 1, #own do
                -- [skirmish.c] the spotter is left alone (an attack order would give it away)
                if cols.hp[k] ~= nil and cols.inside[k] == nil and own[k] ~= mem.sk_spotter then
                    local hits = false
                    for _, c in ipairs(cluster) do
                        if cols.target[k] == c then
                            hits = true
                        end
                    end
                    if hits then
                        str = str + cols.hp[k] * (cols.attack[k] or 0)
                        on = on + 1
                    elseif cf[own[k]] == nil then
                        local t = ai.tile(cols.x[k], cols.y[k])
                        if (cols.attack[k] or 0) > 0 and t ~= nil and (land == 0 or t.continent == land) then
                            cands[#cands + 1] = {id = own[k], s = cols.hp[k] * cols.attack[k]}
                        end
                    end
                end
            end
            local take = 0
            while str * 2 < foe * 3 and take < #cands and on + take < 60 do
                take = take + 1
                str = str + cands[take].s
            end
            if str * 2 >= foe * 3 and take > 0 then
                local by = {}
                for k = 1, take do
                    local c = cluster[(k - 1) % #cluster + 1]
                    by[c] = by[c] or {}
                    by[c][#by[c] + 1] = cands[k].id
                    cf[cands[k].id] = now + 45000
                end
                for _, c in ipairs(ai.keys(by)) do
                    ai.attack(by[c], c)
                end
                mem.cf_sorties = (mem.cf_sorties or 0) + 1
                mem.cf_sent = (mem.cf_sent or 0) + take
                print(string.format("counterfire shooters=%d at=%d,%d foe=%d sent=%d own=%d", #cluster, e.x, e.y, foe, take, str))
            else
                mem.cf_short = (mem.cf_short or 0) + 1
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Tower bait (early.c tower_bait, the playtester's opening; here in every epoch): a citizen hit by a seen enemy
-- soldier, and the citizens within 6 tiles of the attacker, walk to the spot 2 tiles behind the nearest own tower or
-- town centre within 25 tiles (from the attacker), where the raiders that follow come under fire. They are the
-- script's for 40 s (the runners would send them straight back), then released idle.

function M.bait(mem, events, now, epoch)
    if not S.tower_bait or (epoch or 0) > S.tower_bait_epoch then
        return
    end
    mem.bait = mem.bait or {}
    -- release those whose time is up
    local release = {}
    for _, id in ipairs(ai.keys(mem.bait)) do
        if mem.bait[id] <= now then
            release[#release + 1] = id
            mem.bait[id] = nil
        end
    end
    if #release > 0 then
        ai.release(release)
    end
    local attackers = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil then
            local v = ai.unit(e.unit)
            local a = ai.unit(e.attacker)
            if v ~= nil and a ~= nil and v.category == "citizen" and a.category == "soldier" and a.player ~= v.player then
                attackers[a.id] = a
            end
        end
    end
    for _, aid in ipairs(ai.keys(attackers)) do
        local a = attackers[aid]
        -- the citizens hit (upstream: with a soldier within 5 tiles or attacking it), not every citizen near
        local pick = {}
        for _, e in ipairs(events) do
            if e.kind == "attacked" and e.attacker == aid and mem.bait[e.unit] == nil then
                local v = ai.unit(e.unit)
                if v ~= nil and v.category == "citizen" then
                    pick[#pick + 1] = e.unit
                    mem.bait[e.unit] = now   -- marked below
                end
            end
        end
        if #pick > 0 then
            -- the nearest own tower or town centre to the attacker
            local best, bd = nil, nil
            for _, id in ipairs(ai.units{category = "building", armed = true, built = true, near = {a.x, a.y, 25}}) do
                local b = ai.unit(id)
                if b ~= nil then
                    local d = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)
                    if bd == nil or d < bd then
                        best, bd = b, d
                    end
                end
            end
            if best ~= nil then
                local dx, dy = best.x - a.x, best.y - a.y
                local len = math.max(math.abs(dx), math.abs(dy), 1)
                local tx, ty = best.x + dx * 2 // len, best.y + dy * 2 // len
                ai.move(pick, tx, ty, {forced = true})
                for _, id in ipairs(pick) do
                    mem.bait[id] = now + 25000
                end
                mem.baits = (mem.baits or 0) + #pick
                if (mem.baits or 0) <= 20 or now % 60000 < 250 then
                    print(string.format("bait n=%d to=%d,%d attacker=%d baits=%d", #pick, tx, ty, aid, mem.baits))
                end
            else
                for _, id in ipairs(pick) do
                    mem.bait[id] = nil
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Bunkers (bunker.c): the Fortress holds units like a transport (inside they are off the map and out of the
-- population). THE SHELTER (bunkers): from the Copper Age a Fortress by the drop site nearest the base once citizens
-- were hurt three times within two minutes or enemy aircraft are possible (both sides in World War I; two
-- fortresses from 40 citizens); its wood is held back from unit training while it waits (bunker_reserve, econ.lua's
-- reserve); a citizen hit by a seen soldier, and the citizens within 6 tiles of the attacker, go into the nearest
-- finished fortress within 15 tiles with room (ai.load; the micro step); a fortress with nothing hurt within 10
-- tiles for 45 s and no enemy soldiers seen within 10 lets them out after two quiet looks 8 s apart, 3 tiles to a
-- side, the next side when nobody came out (the game takes an unload it cannot do and does nothing), and they are
-- released. THE RESERVE (bunker_reserve): every 10 s with the map full (population at the cap - 1), up to 10 of the
-- force's soldiers at home (within 25 tiles of the base, the force home; 4 stay out) go into the fortress with the
-- most room and training fills the map again; once the population is 4 below the cap with nothing loaded for a
-- minute (losses), or enemy soldiers come within 20 tiles of it, the fullest fortress lets everyone out (not one
-- where citizens shelter) and they are the force's again. THE SHELF (shelve_citizens): every 10 s from the Copper
-- Age, while the population stands at the cap (5 under at most, units queued) and food, wood, gold and iron each
-- lie at shelve_stock or more, up to 20 citizens go into a fortress of the shelf - the nearest gatherers of the
-- resource with the most in stock first, then of any resource at twice shelve_stock, never under max(cap / 8, 40)
-- left out - and no new citizens are trained (econ.lua's citizens wanted); no room in the shelf: one more fortress
-- (up to 8). Out again, a fortress at a time, once one of the four falls under half of shelve_stock or the citizens
-- out are under 3/4 of what is kept out; enemies near keep them in. Not ported: TOWN GUARDS (town_guards, off
-- upstream: a fortress at each front station - fronts.c is not ported). Deviations: the port's "free soldiers" are
-- the force's at home (upstream's are the game's in no group); the room is the view's population (with the units
-- in production, which upstream's room left out); the fortress's spot is ai.find_site by the drop site.

-- the step's part: the fortress wanted and the quiet fortresses' unload
local SIDES = {{0, 3}, {3, 0}, {0, -3}, {-3, 0}, {3, 3}, {-3, 3}, {3, -3}, {-3, -3}}

local function raiders_near(x, y, r)
    return #ai.seen{enemy = true, armed = true, category = "soldier", near = {x, y, r}, limit = 1} > 0
end

-- bk_unload: the holder lets everyone out 3 tiles to a side on land; holding as many at this unload as at the last,
-- it tries the next side
local function unload_fort(mem, f)
    mem.bk_out = mem.bk_out or {}
    local o = mem.bk_out[f.id] or {side = 1, held = -1}
    if o.held == (f.cargo or 0) then
        o.side = o.side % 8 + 1
    end
    o.held = f.cargo or 0
    for _ = 1, 8 do
        local d = SIDES[o.side]
        local t = ai.tile(f.x + d[1], f.y + d[2])
        if t ~= nil and (t.continent or 0) ~= 0 then
            break
        end
        o.side = o.side % 8 + 1
    end
    mem.bk_out[f.id] = o
    local d = SIDES[o.side]
    local code = ai.unload(f.id, f.x + d[1], f.y + d[2])
    return code == "ok"
end

-- the citizens hit within r tiles of (x, y) in the last ms (the micro step's hurt spots)
local function hurt_near(mem, now, x, y, r, ms)
    local n = 0
    for _, h in ipairs(mem.hurt_at or {}) do
        if now - h.t <= ms and math.abs(h.x - x) <= r and math.abs(h.y - y) <= r then
            n = n + 1
        end
    end
    return n
end

-- the units of a hold record (shelter, reserve, shelf) that are not inside go back to the engine; returns how many
local function release_out(rec)
    local keep, out = {}, {}
    for _, id in ipairs(rec.units or {}) do
        local u = ai.unit(id)
        if u ~= nil and u.inside ~= nil then
            keep[#keep + 1] = id
        elseif u ~= nil then
            out[#out + 1] = id
        end
    end
    if #out > 0 then
        ai.release(out)
    end
    rec.units = keep
    return #out
end

local function take_from_group(g, ids)
    local set = {}
    for _, id in ipairs(ids) do
        set[id] = true
    end
    local keep = {}
    for _, id in ipairs(g.units) do
        if not set[id] then
            keep[#keep + 1] = id
        else
            g.aims[id] = nil
        end
    end
    g.units = keep
end

-- a hold record's soldiers or citizens out again (after an unload, or not in within a minute of the load) are the
-- engine's; an empty record is dropped
local function hold_records(mem, recs, now)
    for _, fid in ipairs(ai.keys(recs)) do
        local rec = recs[fid]
        local f = ai.unit(fid)
        if f == nil then
            if #(rec.units or {}) > 0 then
                ai.release(rec.units)
            end
            recs[fid] = nil
        else
            if rec.unloading or now - (rec.in_t or 0) > 60000 then
                release_out(rec)
            end
            if rec.unloading and (f.cargo or 0) == 0 then
                rec.unloading = nil
            end
            if #rec.units == 0 and (f.cargo or 0) == 0 then
                recs[fid] = nil
            end
        end
    end
end

-- the step's part: the fortress wanted, the quiet fortresses' unload, the reserve and the shelf
function M.bunkers(view, mem, order, above, memory)
    if not S.bunkers or view.epoch < 2 or view.town_x < 0 then
        return
    end
    mem.bunker_wood = 0   -- [priority.c] the fortress row (bunker_reserve): the wood it cannot pay yet
    local now = view.time
    mem.hurt = mem.hurt or {}
    local keep = {}
    for _, t in ipairs(mem.hurt) do
        if now - t <= 120000 then
            keep[#keep + 1] = t
        end
    end
    mem.hurt = keep
    mem.shelter = mem.shelter or {}
    mem.reserve = mem.reserve or {}
    mem.shelf = mem.shelf or {forts = {}}
    local forts, nall = {}, 0
    for _, fid in ipairs(ai.units{type = "b_fortress"}) do
        local f = ai.unit(fid)
        if f ~= nil then
            nall = nall + 1
            if f.built then
                forts[#forts + 1] = f
            end
        end
    end
    -- THE FORTRESS WANTED (bk_build, once a minute): citizens hurt or aircraft possible, the reserve near the cap,
    -- the shelf full
    if now - (mem.fort_t or 0) >= 60000 then
        mem.fort_t = now
        local air = false
        if view.epoch >= 9 then
            for _, en in ipairs(view.enemies or {}) do
                if (en.epoch or 0) >= 9 then
                    air = true
                end
            end
        end
        local want = (air or #mem.hurt >= 3) and (view.citizens >= 40 and 2 or 1) or 0
        local room = view.population_cap - view.population
        if S.bunker_reserve and room <= 5 and want < 1 then
            want = 1
        end
        if mem.shelf.want and want < nall + 1 then
            want = math.min(nall + 1, 8)
        end
        local c = V.cost(view, "b_fortress")
        memory.eco.bunker_wood = 0
        if nall < want and c ~= nil then
            if view.stock.wood < c.cost.wood then
                memory.eco.bunker_wood = c.cost.wood   -- can't pay yet: saved for (bunker_reserve)
                mem.bunker_wood = c.cost.wood          -- [priority.c] the fortress row reads it here
            elseif require("ageofai.priority").refuses(view, memory, "b_fortress") == nil then   -- [priority.c]
                -- the drop site nearest the base without a fortress within 10 tiles
                local best, bd = nil, nil
                for _, id in ipairs(ai.units{category = "building", built = true}) do
                    local u = ai.unit(id)
                    if u ~= nil and (u.type == "b_town_center" or u.type == "b_capitol" or u.type == "b_settlement") then
                        local near = false
                        for _, f in ipairs(forts) do
                            if math.abs(f.x - u.x) <= 10 and math.abs(f.y - u.y) <= 10 then
                                near = true
                            end
                        end
                        local d = d2(u.x, u.y, view.town_x, view.town_y)
                        if not near and (bd == nil or d < bd) then
                            best, bd = u, d
                        end
                    end
                end
                if best ~= nil then
                    local x, y = ai.find_site("b_fortress", best.x, best.y, {min = 4, max = 9, safe = true})
                    if x == nil then
                        x, y = ai.find_site("b_fortress", best.x, best.y, {min = 4, max = 9})
                    end
                    if x ~= nil then
                        order({verb = "build", type = "b_fortress", near = "tile", x = x, y = y})
                        mem.forts_ordered = (mem.forts_ordered or 0) + 1
                        print(string.format("bunker fortress at=%d,%d why=%s hurt=%d forts=%d citizens=%d", x, y,
                            mem.shelf.want and "shelf" or air and "aircraft" or #mem.hurt >= 3 and "hurt" or "reserve",
                            #mem.hurt, nall, view.citizens))
                    end
                end
            end
        end
    end
    -- THE SHELTER's quiet release (bk_flush)
    for _, f in ipairs(forts) do
        local sh = mem.shelter[f.id]
        if sh ~= nil then
            if (f.cargo or 0) <= 0 then
                if now - sh.t > 30000 then   -- empty, and nobody still walking in
                    sh.unloading = true
                end
            elseif hurt_near(mem, now, f.x, f.y, 10, 45000) > 0 or raiders_near(f.x, f.y, 10) then
                sh.quiet = nil
            elseif sh.quiet == nil then
                sh.quiet = now
            elseif now - sh.quiet >= 8000 then   -- two quiet looks
                if unload_fort(mem, f) then
                    sh.quiet = nil
                    sh.unloading = true
                    sh.outs = (sh.outs or 0) + 1
                    mem.sheltered_out = (mem.sheltered_out or 0) + (f.cargo or 0)
                    print(string.format("bunker quiet at=%d,%d out=%d outs=%d", f.x, f.y, f.cargo or 0, sh.outs))
                end
            end
        end
    end
    hold_records(mem, mem.shelter, now)
    -- THE RESERVE (bunker_reserve): every 10 s with a finished fortress
    if S.bunker_reserve and #forts > 0 and now - (mem.res_t or 0) >= 10000 then
        mem.res_t = now
        local room = view.population_cap - view.population
        local threat = nil
        for _, f in ipairs(forts) do
            if threat == nil and mem.reserve[f.id] ~= nil and (f.cargo or 0) > 0 and mem.shelter[f.id] == nil
                    and mem.shelf.forts[f.id] == nil and raiders_near(f.x, f.y, 20) then
                threat = f
            end
        end
        if room <= 1 and threat == nil then
            -- the free soldiers at home: the force's members within 25 tiles of the base while the force is home
            local force = G.role(memory, "force")
            local cands = {}
            if force ~= nil and not mem.out then
                local cols = ai.get(force.units, {"x", "y", "inside", "speed"})
                for k, id in ipairs(force.units) do
                    if cols.x[k] ~= nil and cols.inside[k] == nil and (cols.speed[k] or 0) > 0 and id ~= mem.sk_spotter
                            and not M.committed(mem, id, now) and d2(cols.x[k], cols.y[k], view.town_x, view.town_y) <= 625 then
                        cands[#cands + 1] = {id = id, x = cols.x[k], y = cols.y[k]}
                    end
                end
            end
            -- the fortress with the most room (not the shelf's)
            local best, space = nil, 0
            for _, f in ipairs(forts) do
                local c = ai.class(f.type)
                local rec = mem.reserve[f.id]
                local walking = rec ~= nil and math.max(#rec.units - (f.cargo or 0), 0) or 0
                local s = (c and c.transport or 0) - (f.cargo or 0) - walking
                if s > space and mem.shelf.forts[f.id] == nil then
                    best, space = f, s
                end
            end
            local n = math.min(#cands - 4, 10, space)
            if best ~= nil and n > 0 then
                local fx, fy = best.x, best.y
                table.sort(cands, function(a, b)
                    local da, db = d2(a.x, a.y, fx, fy), d2(b.x, b.y, fx, fy)
                    if da ~= db then
                        return da < db
                    end
                    return a.id < b.id
                end)
                local ids = {}
                for k = 1, n do
                    ids[k] = cands[k].id
                end
                local code = ai.load(ids, best.id)
                if code == "ok" then
                    take_from_group(force, ids)
                    local rec = mem.reserve[best.id] or {units = {}}
                    for _, id in ipairs(ids) do
                        rec.units[#rec.units + 1] = id
                    end
                    rec.in_t = now
                    mem.reserve[best.id] = rec
                    mem.res_load_t = now
                    mem.res_in = (mem.res_in or 0) + n
                    print(string.format("reserve in room=%d n=%d at=%d,%d holds=%d in=%d out=%d", room, n, fx, fy, best.cargo or 0,
                        mem.res_in, mem.res_out or 0))
                end
            end
        elseif threat ~= nil or (room >= 4 and (mem.res_load_t == nil or now - mem.res_load_t >= 60000)) then
            local best, held = threat, threat ~= nil and (threat.cargo or 0) or 0
            if threat == nil then
                for _, f in ipairs(forts) do
                    if mem.reserve[f.id] ~= nil and (f.cargo or 0) > held and mem.shelter[f.id] == nil and mem.shelf.forts[f.id] == nil then
                        best, held = f, f.cargo
                    end
                end
            end
            if best ~= nil and unload_fort(mem, best) then
                mem.reserve[best.id].unloading = true
                mem.res_out = (mem.res_out or 0) + held
                print(string.format("reserve out room=%d%s at=%d,%d n=%d in=%d out=%d", room, threat ~= nil and " enemies_near" or "",
                    best.x, best.y, held, mem.res_in or 0, mem.res_out))
            end
        end
    end
    hold_records(mem, mem.reserve, now)
    -- THE SHELF (shelve_citizens): every 10 s from the Copper Age
    local shelf = mem.shelf
    if S.shelve_citizens and now - (mem.shelf_t or 0) >= 10000 then
        mem.shelf_t = now
        local st = view.stock
        local function rich(k)
            return st.food >= k and st.wood >= k and st.gold >= k and st.iron >= k
        end
        local keep_out = math.max(view.population_cap // 8, 40)
        local inside = 0   -- citizens in the fortresses (the shelf's and the shelter's)
        for _, f in ipairs(forts) do
            if shelf.forts[f.id] ~= nil or mem.shelter[f.id] ~= nil then
                inside = inside + (f.cargo or 0)
            end
        end
        local out = view.citizens - inside
        local room = view.population_cap - view.population
        local queued = 0
        for _, c in ipairs(view.costs) do
            if not c.building then
                queued = queued + c.in_production
            end
        end
        shelf.want = nil
        if not rich(S.shelve_stock // 2) or out < keep_out * 3 // 4 then
            -- out again: the fullest of the shelf's, if quiet
            local best, held = nil, 0
            for _, f in ipairs(forts) do
                local rec = shelf.forts[f.id]
                if rec ~= nil then
                    if (f.cargo or 0) <= 0 and #rec.units == 0 then
                        shelf.forts[f.id] = nil   -- empty: an ordinary fortress again
                    elseif (f.cargo or 0) > held and not raiders_near(f.x, f.y, 15) then
                        best, held = f, f.cargo
                    end
                end
            end
            if best ~= nil and unload_fort(mem, best) then
                shelf.forts[best.id].unloading = true
                shelf.n_out = (shelf.n_out or 0) + held
                print(string.format("shelf out n=%d at=%d,%d out=%d keep=%d stock=%d/%d/%d/%d in=%d out_total=%d", held, best.x, best.y,
                    out, keep_out, st.food, st.wood, st.gold, st.iron, shelf.n_in or 0, shelf.n_out))
            end
        elseif room <= 5 and queued >= 1 and rich(S.shelve_stock) and out > keep_out then
            shelf.on_t = now
            -- a fortress of the shelf with room, else an empty one (not the shelter's, not the reserve's)
            local best, space = nil, 0
            for _, f in ipairs(forts) do
                local c = ai.class(f.type)
                local rec = shelf.forts[f.id]
                local walking = rec ~= nil and math.max(#rec.units - (f.cargo or 0), 0) or 0
                local s = (c and c.transport or 0) - (f.cargo or 0) - walking
                local skip = mem.shelter[f.id] ~= nil or (rec == nil and ((f.cargo or 0) > 0 or mem.reserve[f.id] ~= nil))
                if not skip and s > space + (rec ~= nil and -1 or 0) then
                    best, space = f, s
                end
            end
            if best == nil or space < 5 then
                shelf.want = true
            else
                local n = math.min(out - keep_out, 20, space)
                local richest = "food"   -- the resource with the most in stock: its gatherers first
                for _, k in ipairs({"wood", "stone", "gold", "iron"}) do
                    if st[k] > st[richest] then
                        richest = k
                    end
                end
                local cands = ai.units{category = "citizen", action = "gather", near = {best.x, best.y, 60}, limit = 80}
                local cols = ai.get(cands, {"x", "held", "inside", "target"})
                local pick, picked, kinds = {}, {}, {}
                for pass = 0, 1 do
                    for k, id in ipairs(cands) do   -- nearest first
                        if #pick >= n then
                            break
                        end
                        if cols.x[k] ~= nil and cols.held[k] == "engine" and cols.inside[k] == nil and cols.target[k] ~= nil and not picked[id] then
                            local kind = kinds[k]
                            if kind == nil then
                                local r = ai.unit(cols.target[k])
                                kind = r ~= nil and r.kind or false
                                kinds[k] = kind
                            end
                            -- never the scarce resource's gatherers (under twice shelve_stock)
                            if kind and (pass == 0) == (kind == richest) and (st[kind] or 0) >= 2 * S.shelve_stock then
                                pick[#pick + 1] = id
                                picked[id] = true
                            end
                        end
                    end
                end
                if #pick > 0 then
                    local code = ai.load(pick, best.id)
                    if code == "ok" then
                        local rec = shelf.forts[best.id] or {units = {}}
                        for _, id in ipairs(pick) do
                            rec.units[#rec.units + 1] = id
                        end
                        rec.in_t = now
                        shelf.forts[best.id] = rec
                        shelf.n_in = (shelf.n_in or 0) + #pick
                        print(string.format("shelf in room=%d n=%d at=%d,%d holds=%d out=%d keep=%d stock=%d/%d/%d/%d in=%d out_total=%d",
                            room, #pick, best.x, best.y, best.cargo or 0, out, keep_out, st.food, st.wood, st.gold, st.iron, shelf.n_in, shelf.n_out or 0))
                    end
                end
            end
        end
    end
    hold_records(mem, shelf.forts, now)
    -- shelf_wanted (econ.lua): while the shelf works or holds citizens, no more citizens than there are
    local held = 0
    for _, f in ipairs(forts) do
        if shelf.forts[f.id] ~= nil then
            held = held + (f.cargo or 0)
        end
    end
    memory.shelf_hold = (shelf.on_t ~= nil and now - shelf.on_t < 30000) or held > 0
end

-- the micro step's part: the citizens hit go into a fortress near
function M.shelter(mem, events, now)
    if not S.bunkers then
        return
    end
    mem.hurt = mem.hurt or {}
    mem.hurt_at = mem.hurt_at or {}
    mem.shelter = mem.shelter or {}
    local attackers = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" then
            local v = ai.unit(e.unit)
            if v ~= nil and v.category == "citizen" then
                mem.hurt[#mem.hurt + 1] = now
                mem.hurt_at[#mem.hurt_at + 1] = {x = v.x, y = v.y, t = now}
                if #mem.hurt_at > 64 then
                    table.remove(mem.hurt_at, 1)
                end
                if e.attacker ~= nil then
                    local a = ai.unit(e.attacker)
                    if a ~= nil and a.category == "soldier" and a.player ~= v.player then
                        attackers[a.id] = a
                    end
                end
            end
        end
    end
    for _, aid in ipairs(ai.keys(attackers)) do
        local a = attackers[aid]
        local best, bd = nil, nil
        for _, fid in ipairs(ai.units{type = "b_fortress", built = true, near = {a.x, a.y, 15}}) do
            local f = ai.unit(fid)
            local c = f ~= nil and ai.class(f.type) or nil
            -- not the shelf's fortresses (the shelter and the reserve keep off them)
            if f ~= nil and c ~= nil and (f.cargo or 0) < (c.transport or 0) and (mem.shelf == nil or mem.shelf.forts[fid] == nil) then
                local d = d2(f.x, f.y, a.x, a.y)
                if bd == nil or d < bd then
                    best, bd = f, d
                end
            end
        end
        if best ~= nil then
            local pick = {}
            for _, id in ipairs(ai.units{category = "citizen", near = {a.x, a.y, 6}}) do
                local u = ai.unit(id)
                if u ~= nil and u.inside == nil and (mem.bait == nil or mem.bait[id] == nil) then
                    pick[#pick + 1] = id
                end
            end
            if #pick > 0 then
                local code, reason, n = ai.load(pick, best.id)
                if code == "ok" then
                    local sh = mem.shelter[best.id] or {units = {}, t = now}
                    sh.t = now
                    sh.in_t = now
                    sh.quiet = nil
                    sh.unloading = nil
                    for _, id in ipairs(pick) do
                        sh.units[#sh.units + 1] = id
                    end
                    mem.shelter[best.id] = sh
                    mem.sheltered = (mem.sheltered or 0) + n
                end
            end
        end
    end
end

-- whether a unit is committed to a counter-fire sortie (the force's orders leave it alone)
function M.committed(mem, id, now)
    return mem.cf ~= nil and mem.cf[id] ~= nil and mem.cf[id] > now
end

return M

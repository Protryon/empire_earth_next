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
                    -- military account alone
                    n = math.min(n, budget.civil[r] // c.cost[r])
                end
            end
            if n > 0 and c.requested + c.in_production == 0 then
                order({verb = "train", type = t, n = n})
                mem.siege_trained = (mem.siege_trained or 0) + n
                return c
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
        elseif not (u.action == "move" and u.tx ~= nil and math.abs(u.tx - px) < 2 and math.abs(u.ty - py) < 2) then
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
            else
                for _, id in ipairs(pick) do
                    mem.bait[id] = nil
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Bunkers (bunker.c): from the Copper Age a Fortress by the drop site nearest the base once citizens were hurt
-- three times within two minutes (two fortresses from 40 citizens); a citizen hit by a seen soldier, and the
-- citizens within 6 tiles of the attacker, go into the nearest finished fortress within 15 tiles with room
-- (ai.load: the fortress is a transport class); a fortress quiet for 8 s (no citizen hit within 10 tiles) lets them
-- out 3 tiles to a side, the next side when nobody came out, and they are released.

-- the step's part: the fortress wanted and the quiet fortresses' unload
function M.bunkers(view, mem, order, above)
    if not S.bunkers or view.epoch < 2 or view.town_x < 0 then
        return
    end
    mem.hurt = mem.hurt or {}
    local keep = {}
    for _, t in ipairs(mem.hurt) do
        if view.time - t <= 120000 then
            keep[#keep + 1] = t
        end
    end
    mem.hurt = keep
    local forts = ai.units{type = "b_fortress"}
    local want = #mem.hurt >= 3 and (view.citizens >= 40 and 2 or 1) or 0
    if #forts < want and view.time - (mem.fort_t or 0) >= 60000 and V.cost(view, "b_fortress") ~= nil then
        local c = V.cost(view, "b_fortress")
        if above.wood >= c.cost.wood and above.stone >= c.cost.stone then
            local best, bd = nil, nil
            for _, id in ipairs(ai.units{category = "building", built = true}) do
                local u = ai.unit(id)
                if u ~= nil and (u.type == "b_town_center" or u.type == "b_capitol" or u.type == "b_settlement") then
                    local near = false
                    for _, fid in ipairs(forts) do
                        local f = ai.unit(fid)
                        if f ~= nil and math.abs(f.x - u.x) <= 10 and math.abs(f.y - u.y) <= 10 then
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
                    mem.fort_t = view.time
                    mem.forts_ordered = (mem.forts_ordered or 0) + 1
                    print(string.format("bunker fortress at=%d,%d hurt=%d forts=%d", x, y, #mem.hurt, #forts))
                end
            end
        end
    end
    mem.shelter = mem.shelter or {}
    for _, fid in ipairs(forts) do
        local f = ai.unit(fid)
        local sh = mem.shelter[fid]
        if f ~= nil and f.built and (f.cargo or 0) > 0 and sh ~= nil then
            local hits_near = false
            for _, h in ipairs(mem.hurt_at or {}) do
                if view.time - h.t < 8000 and math.abs(h.x - f.x) <= 10 and math.abs(h.y - f.y) <= 10 then
                    hits_near = true
                end
            end
            if not hits_near and view.time - sh.t >= 8000 then
                local sides = {{0, 3}, {3, 0}, {0, -3}, {-3, 0}, {3, 3}, {-3, 3}, {3, -3}, {-3, -3}}
                sh.side = ((sh.side or 0) % #sides) + 1
                local d = sides[sh.side]
                ai.unload(fid, f.x + d[1], f.y + d[2])
                sh.t = view.time
                sh.outs = (sh.outs or 0) + 1
                if sh.outs >= 8 then
                    ai.release(sh.units or {})
                    sh.units = {}
                end
            end
        elseif f ~= nil and (f.cargo or 0) == 0 and sh ~= nil then
            ai.release(sh.units or {})
            mem.shelter[fid] = nil
            mem.sheltered_out = (mem.sheltered_out or 0) + 1
        end
    end
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
            if f ~= nil and c ~= nil and (f.cargo or 0) < (c.transport or 0) then
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

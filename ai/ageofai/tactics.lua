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
                    n = math.min(n, budget.avail[r] // c.cost[r])
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
                if cols.hp[k] ~= nil and cols.inside[k] == nil then
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

-- whether a unit is committed to a counter-fire sortie (the force's orders leave it alone)
function M.committed(mem, id, now)
    return mem.cf ~= nil and mem.cf[id] ~= nil and mem.cf[id] > now
end

return M

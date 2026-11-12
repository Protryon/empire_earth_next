-- Age of AI port, THE DETACHMENTS (upstream dll/ai/armies.c MINE RAIDS, HIT AND RUN, THE RAIDERS, THE RIDERS' LOOK,
-- THE SITES' MEMORY of towns.c; config/ageofai.ini towns=...|mine_raids, off upstream; UPSTREAM has the map and the
-- licence: GPL-3.0, Grille and the Age of AI contributors). require("ageofai.detach"), called from army.lua's step
-- after the force; settings.lua `mine_raids` switches it on.
--
-- MINE RAIDS (the player 2026-11-08: "small detachments that stay until the mine is secured"): the force goes out
-- 2-3.5 times a game, too rarely to take mines. Every AR_DET_LOOK, none out and AR_DET_PAUSE after the last:
-- AR_DET_SIZE soldiers (half of those at home at most, AR_DET_MIN at home needed) leave the force for the enemy drop
-- site by a mine it knows on its own land that they beat by ar_go_pct, the nearest such (only what it has seen). The
-- site down: what else it sees within 22 tiles, then the town; they stand over it until it is secured (an own drop
-- site and two towers standing there), AR_DET_HOLD at most, and go back to the force. They turn back as the force
-- does (before contact only on a clear deficit, AR_FAR_PCT twice; in contact when the fight itself goes badly: the
-- trade over the last AR_TRADE_N judgements, once AR_DET_TRADE_FLOOR of their hit points are gone).
-- HIT AND RUN (the player: "just going in killing a few citizens and getting out again is worth much already"): when
-- its army is outmatched, too few are at home for a detachment or it would beat no mine: AR_HIT_MIN to AR_HIT_SIZE
-- cavalry (mounted archers first; ar_det_train keeps AR_GROUP at home) ride to the least defended mine it knows that
-- no tower it knows covers (a covered one is asked of the army: site_defended), not hit in the last 6 minutes; there
-- each attacks the nearest enemy citizen it sees, on to the next mine within 12 tiles after AR_HIT_NEXT with nobody
-- in sight. They leave after AR_HIT_TIME, with nobody left to kill for 20 s, once AR_HIT_LOSS % of their hit points
-- are gone or as many enemy fighters as they are stand within 10 tiles.
-- THE RAIDERS: AR_GROUP cavalry stay out of the force at home (every 15 s the force's cavalry at home are taken),
-- kept up by ar_det_train once the economy allows it (two farms, not recovering, AR_HIT_ECON % of the citizens
-- wanted, or the AI outmatched): the best kind it can train, two every 30 s, from the military account; nothing
-- trains: a Stable (5 minutes apart). Handed back to the force when enemies are in the base.
-- THE RIDERS' LOOK: a mine is only known if someone has seen it. A rider alone, the fastest cavalry at home, goes to
-- the land not looked at for 10 minutes that lies nearest to the enemy buildings it knows (at first nearest to its
-- own base); two out at a time, a new one a minute, while fewer than AR_LOOK_ENOUGH mines to raid are known; on to
-- the next place until killed or its time is up.
-- THE SITES' MEMORY: a site a detachment failed at is the force's (ar_site_asked in army.lua: the push goes there
-- without waiting for the attack time), tried again by a detachment after SITE_RETRY; a hit one rests 6 minutes.
--
-- DEVIATIONS (engine differences, each marked below): no crew (upstream's six citizens behind the detachment and
-- tn_claim's town: towns.lua founds its own towns; the detachment holds the cleared site until an own drop site and
-- two towers stand within 18 tiles, or AR_DET_HOLD); the cavalry's kind from the class name (ai.class has no family);
-- the best kind to train by hit points x attack (no tech level in ai.class); "looked at" is the module's own record
-- of the riders' cells and what is visible now (no per-tile last-seen time); the base threatened is
-- view.threat_strength > 0; a target's life from the memory (fog-honest; upstream read the unit).
local V = require("lib.view")
local G = require("lib.groups")
local S = require("ageofai.settings")
local AT = require("ageofai.attack")
local M = {}

local AR_DET_SIZE, AR_DET_LOOK, AR_DET_HOLD, AR_DET_TARGETS = 20, 15000, 900000, 32
local AR_DET_MIN, AR_DET_PAUSE = 30, 180000
local AR_HIT_SIZE, AR_HIT_MIN, AR_HIT_ECON, AR_HIT_PAUSE, AR_HIT_TIME, AR_HIT_LOSS = 12, 10, 50, 90000, 90000, 40
local AR_HIT_NEAR, AR_HIT_NEXT, AR_HIT_MINES, AR_HIT_R, AR_DET_TRADE_FLOOR = 6, 8000, 4, 16, 50
local AR_GROUP = 16
local AR_LOOK_N, AR_LOOK_EVERY, AR_LOOK_STALE, AR_LOOK_CELL = 2, 60000, 600000, 12
local AR_LOOK_ENOUGH, AR_LOOK_LIFE, AR_LOOK_LEG = 3, 480000, 120000
local SITE_R, SITE_RETRY, SITE_MAX, SITE_HIT_REST = 14, 1200000, 64, 360000
local AR_FAR_PCT, AR_CONTACT_MIN, AR_TRADE_N, AR_TRADE_MIN, AR_TRADE_BAD = 70, 3, 8, 4, 150
local SG_FOE = 20
local HOME_R = 25
local DROPS = {b_town_center = true, b_capitol = true, b_settlement = true}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function st(mem)
    mem.det = mem.det or {state = 0, mode = 0, units = {}, hp = {}, foe = {}, dealt = {}, lost = {}, vis = {}, tgt = {},
                          sites = {}, look = {}, looked = {}, known = 0,
                          sent = 0, cleared = 0, founded = 0, secured = 0, losing = 0, no_town = 0, timed_out = 0,
                          killed = 0, trained = 0, look_sent = 0, look_died = 0, look_home = 0}
    return mem.det
end

-- the enemy strength known within r of a tile (hit points x attack of the fighters and defences)
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

-- ex_outmatched: the enemy army known (the memory's armed units seen within 10 minutes) stronger than its own
local function outmatched(view)
    local theirs = 0
    for _, m in ipairs(ai.memory{since = view.time - 600000}) do
        if not m.building then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                theirs = theirs + m.hp * c.attack
            end
        end
    end
    return theirs > ai.strength(ai.units{category = "soldier"})
end

-- HIT AND RUN: how fit a type is for a raid - mounted archers 3, cavalry with guns 2, other cavalry 1, the rest 0
-- (DEVIATION: from the class name; upstream reads the family)
function M.fast(t)
    t = t or ""
    if string.find(t, "cavalry_archer", 1, true) or string.find(t, "chariot_archer", 1, true)
            or string.find(t, "elephant_archer", 1, true) then
        return 3
    end
    if string.sub(t, 1, 3) == "cav" then
        if string.find(t, "gun", 1, true) or string.find(t, "cuirassier", 1, true) then
            return 2
        end
        return 1
    end
    return 0
end

-- a mine seen within r of a tile (the nearest), or nil; `avoid` lists {x, y} within 3 tiles of which none counts
local function mine_near(x, y, r, avoid)
    local list = ai.resources{source = "mine", near = {x, y, r}}
    for _, id in ipairs(list) do
        local m = ai.unit(id)   -- (no ai.resource binding: the unit record carries kind, amount, x, y)
        if m ~= nil and m.category == "resource" and (m.amount or 0) > 0 then
            local skip = false
            for _, a in ipairs(avoid or {}) do
                if d2(m.x, m.y, a.x, a.y) <= 9 then
                    skip = true
                end
            end
            if not skip then
                return m
            end
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE SITES' MEMORY (towns.c): the mines a detachment tried, failed at, found covered or hit

local function site_at(d, x, y, add)
    for _, s in ipairs(d.sites) do
        if d2(s.x, s.y, x, y) <= SITE_R * SITE_R then
            return s
        end
    end
    if not add then
        return nil
    end
    local s = {x = x, y = y, tries = 0, fails = 0, t = 0, foe_hp = 0, hit_t = 0}
    if #d.sites < SITE_MAX then
        d.sites[#d.sites + 1] = s
    else
        d.sites[1] = s
    end
    return s
end

local function site_failed(d, x, y, foe_hp, now)
    local s = site_at(d, x, y, true)
    s.fails, s.t, s.foe_hp = s.fails + 1, now, foe_hp
    print(string.format("detach site_failed at=%d,%d fails=%d tries=%d foe=%d", s.x, s.y, s.fails, s.tries, foe_hp))
end

local function site_defended(d, x, y, now)
    local s = site_at(d, x, y, true)
    if s.fails > 0 then
        return
    end
    s.fails, s.t = 1, now
    print(string.format("detach site_defended at=%d,%d", s.x, s.y))
end

local function site_hit_ok(d, x, y, now)
    local s = site_at(d, x, y, false)
    return s == nil or s.hit_t == 0 or now - s.hit_t >= SITE_HIT_REST
end

local function site_for_det(d, x, y, now)
    local s = site_at(d, x, y, false)
    return s == nil or s.fails == 0 or now - s.t >= SITE_RETRY
end

-- the enemy drop sites seen by a mine on the own land, nearest home first (AR_DET_TARGETS at most)
local function drop_sites(view, hx, hy)
    local home = ai.tile(hx, hy)
    local land = home ~= nil and home.continent or 0
    local out = {}
    for _, id in ipairs(ai.seen{enemy = true, category = "building", near = {hx, hy, 300}}) do
        local u = ai.unit(id)
        if u ~= nil and DROPS[u.type] then
            local t = ai.tile(u.x, u.y)
            if t ~= nil and (land == 0 or t.continent == land) and mine_near(u.x, u.y, 10) ~= nil then
                out[#out + 1] = {id = u.id, x = u.x, y = u.y, player = u.player, d = d2(u.x, u.y, hx, hy)}
            end
        end
    end
    table.sort(out, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.id < b.id
    end)
    while #out > AR_DET_TARGETS do
        out[#out] = nil
    end
    return out
end

-- army.lua: the drop site seen that a detachment failed at or found covered - the force's target without waiting for
-- the attack time (ar_site_asked)
function M.site_asked(view, mem, hx, hy)
    local d = mem.det
    if d == nil then
        return nil
    end
    for _, c in ipairs(drop_sites(view, hx, hy)) do
        local s = site_at(d, c.x, c.y, false)
        if s ~= nil and s.fails > 0 then
            return c
        end
    end
    return nil
end

function M.out(mem)
    return mem.det ~= nil and mem.det.state ~= 0
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE RAIDERS: the economy first, the cavalry trained, kept at home out of the force

local function econ_ok(view, memory, budget)
    local farms = V.buildings(view, "b_farm")
    if farms == nil or farms.finished < 2 then
        return false
    end
    local eco = memory.eco or {}
    if eco.recovering then   -- recovering (econ.c)
        return false
    end
    local goal = (budget and budget.citizens_wanted) or view.citizens
    if view.citizens * 100 >= goal * AR_HIT_ECON then
        return true
    end
    return outmatched(view)
end

local function train(view, memory, d, have, order, budget)
    local now = view.time
    if have >= AR_GROUP or (d.train_t ~= nil and now - d.train_t < 30000) then
        return
    end
    d.train_t = now
    if not econ_ok(view, memory, budget) then
        return
    end
    local n = math.min(2, AR_GROUP - have)
    for rank = 3, 1, -1 do
        local best, bs = nil, 0
        for _, c in ipairs(view.costs) do
            if c.military and not c.building and c.producer ~= nil and c.producers > 0 and M.fast(c.type) == rank then
                local cls = ai.class(c.type)
                local score = cls ~= nil and (cls.hp or 0) * (cls.attack or 0) or 0   -- DEVIATION: no tech level
                if score > bs then
                    best, bs = c, score
                end
            end
        end
        if best ~= nil then
            for _, r in ipairs(V.resources) do
                if best.cost[r] > 0 then
                    n = math.min(n, (budget.avail[r] or 0) // best.cost[r])
                end
            end
            if n <= 0 then
                return   -- the account can't pay: it waits, not a worse kind
            end
            order({verb = "train", type = best.type, n = n})
            d.trained = d.trained + n
            print(string.format("detach train type=%s n=%d have=%d", best.type, n, have))
            return
        end
    end
    local stable = V.buildings(view, "b_stable")
    if V.cost(view, "b_stable") ~= nil and (stable == nil or stable.finished + stable.unfinished + stable.queued == 0)
            and (d.stable_t == nil or now - d.stable_t >= 300000) then
        d.stable_t = now
        order({verb = "build", type = "b_stable", near = "town"})
        print("detach stable")
    end
end

local function riders_tick(view, memory, mem, d, force, order, budget)
    local now = view.time
    local riders = G.role(memory, "riders")
    if view.threat_strength > 0 then   -- enemies in the base: back to the force
        if riders ~= nil then
            G.add(memory, force, riders.units)
            G.disband(memory, riders)
        end
        return
    end
    if d.group_t ~= nil and now - d.group_t < AR_DET_LOOK then
        return
    end
    d.group_t = now
    if not econ_ok(view, memory, budget) then
        return
    end
    local have = riders ~= nil and #riders.units or 0
    if have < AR_GROUP and not mem.out then
        local hx, hy = mem.home_x, mem.home_y
        local take = {}
        for _, id in ipairs(force.units) do
            if have + #take >= AR_GROUP then
                break
            end
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil and M.fast(u.type) > 0 then
                take[#take + 1] = id
            end
        end
        if #take > 0 then
            if riders == nil then
                riders = G.create(memory, {task = "defend", role = "riders", x = hx, y = hy, radius = S.ar_home_r, manual = true})
            end
            G.add(memory, riders, take)
            ai.move(take, hx, hy, {forced = true})
            have = have + #take
        end
    end
    train(view, memory, d, have + ((d.state ~= 0 and d.mode == 1) and #d.units or 0), order, budget)
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE RIDERS' LOOK

local function cell_key(x, y)
    return string.format("%d_%d", x, y)
end

-- the cell of AR_LOOK_CELL tiles not looked at for AR_LOOK_STALE on the home land, no threat, not another rider's,
-- nearest to an enemy building known (else to home)
local function look_cell(view, d, self, hx, hy)
    local map = ai.map()
    local home = ai.tile(hx, hy)
    local land = home ~= nil and home.continent or 0
    local foes = {}
    for _, m in ipairs(ai.memory{buildings = true, limit = 32}) do
        foes[#foes + 1] = m
    end
    local best, bx, by = nil, nil, nil
    local now = view.time
    local half = AR_LOOK_CELL // 2
    for y = half, map.height - 1, AR_LOOK_CELL do
        for x = half, map.width - 1, AR_LOOK_CELL do
            local key = cell_key(x, y)
            local t = ai.tile(x, y)
            local looked = d.looked[key]
            if t ~= nil and t.visible then   -- DEVIATION: seen now counts as looked at now
                looked = now
                d.looked[key] = now
            end
            local ok = t ~= nil and t.land and (land == 0 or t.continent == land)
                and (looked == nil or now - looked > AR_LOOK_STALE) and ai.danger(x, y) == 0
            if ok then
                for k, l in ipairs(d.look) do
                    if k ~= self and l.id ~= nil and d2(x, y, l.tx, l.ty) < 16 * 16 then
                        ok = false
                    end
                end
            end
            if ok then
                local dist = nil
                if #foes == 0 then
                    dist = d2(x, y, hx, hy)
                else
                    for _, f in ipairs(foes) do
                        local q = d2(x, y, f.x, f.y)
                        if dist == nil or q < dist then
                            dist = q
                        end
                    end
                end
                if best == nil or dist < best then
                    best, bx, by = dist, x, y
                end
            end
        end
    end
    return bx, by
end

local function look_tick(view, memory, mem, d, force, budget)
    local now = view.time
    local hx, hy = mem.home_x, mem.home_y
    local look = G.role(memory, "look")
    local busy = 0
    for k = 1, AR_LOOK_N do
        d.look[k] = d.look[k] or {}
        local l = d.look[k]
        if l.id ~= nil then
            local u = ai.unit(l.id)
            if u == nil then
                l.id = nil
                d.look_died = d.look_died + 1
            else
                busy = busy + 1
                if d2(u.x, u.y, l.tx, l.ty) <= 16 then
                    d.looked[cell_key(l.tx, l.ty)] = now
                end
                if d2(u.x, u.y, l.tx, l.ty) > 16 and now - l.leg < AR_LOOK_LEG then
                    -- on its way
                else
                    local nx, ny = nil, nil
                    if now - l.t < AR_LOOK_LIFE then
                        nx, ny = look_cell(view, d, k, hx, hy)
                    end
                    if nx ~= nil then
                        l.tx, l.ty, l.leg = nx, ny, now
                        ai.move({l.id}, nx, ny, {forced = true})
                    else
                        -- its time is up: home, the force's again
                        G.add(memory, force, {l.id})
                        l.id = nil
                        d.look_home = d.look_home + 1
                        busy = busy - 1
                    end
                end
            end
        end
    end
    if look ~= nil and #look.units == 0 then
        G.disband(memory, look)
        look = nil
    end
    if d.known >= AR_LOOK_ENOUGH or busy >= AR_LOOK_N or (d.look_t ~= nil and now - d.look_t < AR_LOOK_EVERY) then
        return
    end
    if view.threat_strength > 0 or not econ_ok(view, memory, budget) then
        return
    end
    d.look_t = now
    -- the fastest cavalry at home: the force's, the raiders'
    local sc, fast = nil, 0
    local pool = {}
    if not mem.out then
        for _, id in ipairs(force.units) do
            pool[#pool + 1] = id
        end
    end
    local riders = G.role(memory, "riders")
    if riders ~= nil then
        for _, id in ipairs(riders.units) do
            pool[#pool + 1] = id
        end
    end
    for _, id in ipairs(pool) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and M.fast(u.type) > 0 and (u.speed or 0) > fast then
            sc, fast = u, u.speed
        end
    end
    local slot = nil
    for k = 1, AR_LOOK_N do
        if slot == nil and d.look[k].id == nil then
            slot = k
        end
    end
    if sc == nil or slot == nil then
        return
    end
    local tx, ty = look_cell(view, d, slot, hx, hy)
    if tx == nil then
        return
    end
    if look == nil then
        look = G.create(memory, {task = "defend", role = "look", x = hx, y = hy, radius = 0, manual = true})
    end
    G.add(memory, look, {sc.id})
    d.look[slot] = {id = sc.id, tx = tx, ty = ty, t = now, leg = now}
    d.look_sent = d.look_sent + 1
    ai.move({sc.id}, tx, ty, {forced = true})
    print(string.format("detach look to=%d,%d known=%d sent=%d died=%d home=%d", tx, ty, d.known, d.look_sent, d.look_died, d.look_home))
end

-- ---------------------------------------------------------------------------------------------------------------
-- The detachment

local function members(d)
    local recs, sx, sy, n, hp = {}, 0, 0, 0, 0
    local keep, keep_hp = {}, {}
    for k, id in ipairs(d.units) do
        local u = ai.unit(id)
        if u ~= nil then
            keep[#keep + 1] = id
            keep_hp[#keep_hp + 1] = d.hp[k]
            recs[#recs + 1] = u
            sx, sy, n, hp = sx + u.x, sy + u.y, n + 1, hp + u.hp
        else
            d.dead_hp = (d.dead_hp or 0) + (d.hp[k] or 0)
        end
    end
    d.units, d.hp = keep, keep_hp
    if n == 0 then
        return recs, nil, nil, 0
    end
    return recs, sx // n, sy // n, hp
end

local function own_strength(ids)
    return ai.strength(ids)
end

-- the orders: a move to the destination (fight: an attack-move), idle members only when asked
local function order_det(d, idle_only, fight)
    local ids = {}
    for _, id in ipairs(d.units) do
        local u = ai.unit(id)
        if u ~= nil and (not idle_only or u.action == "idle") then
            ids[#ids + 1] = id
        end
    end
    if #ids > 0 then
        ai.move(ids, d.dest_x, d.dest_y, {forced = not fight})
    end
end

-- over: its soldiers back to the force (free again: the next tick holds them)
local function det_end(view, memory, mem, d, force, why, failed)
    local now = view.time
    if failed and d.mode == 0 then
        site_failed(d, d.origin_x, d.origin_y, d.foe_hp or 0, now)
    end
    if d.mode == 1 then
        -- out at once, a plain move: they don't stop to fight on the way
        if #d.units > 0 then
            ai.move(d.units, mem.home_x, mem.home_y, {forced = true})
        end
        site_at(d, d.origin_x, d.origin_y, true).hit_t = now
    end
    print(string.format("detach end at=%d,%d left=%d why=%s", d.dest_x, d.dest_y, #d.units, why))
    local g = G.role(memory, "det")
    if g ~= nil then
        if #g.units > 0 then
            G.add(memory, force, g.units)
        end
        G.disband(memory, g)
    end
    d.state, d.mode_last, d.units, d.hp, d.last = 0, d.mode, {}, {}, now
end

-- the fight since the last judgement: the hit points its members lost and those it took off the enemy seen round
-- its middle; members fighting
local function trade(d, recs, mid_x, mid_y)
    local fighting, dealt, lost = 0, 0, d.dead_hp or 0
    d.dead_hp = 0
    for k, u in ipairs(recs) do
        if (d.hp[k] or u.hp) > u.hp then
            lost = lost + d.hp[k] - u.hp
        end
        d.hp[k] = u.hp
        if u.action == "attack" then
            fighting = fighting + 1
        end
    end
    for _, id in ipairs(ai.keys(d.foe)) do
        local u = ai.unit(id)
        local hp = u ~= nil and u.hp or 0
        if d.foe[id] > hp then
            dealt = dealt + d.foe[id] - hp
        end
    end
    d.foe = {}
    local n = 0
    for _, id in ipairs(ai.seen{enemy = true, near = {mid_x, mid_y, SG_FOE}}) do
        if n >= 128 then
            break
        end
        local u = ai.unit(id)
        if u ~= nil and (u.armed or u.category == "building") then
            d.foe[id] = u.hp
            n = n + 1
        end
    end
    return fighting, dealt, lost
end

-- the enemy fighters seen within r of a tile
local function enemy_band(x, y, r)
    local n = 0
    for _, id in ipairs(ai.seen{enemy = true, armed = true, category = "soldier", near = {x, y, r}}) do
        n = n + 1
    end
    return n
end

local function det_start(view, memory, mem, d, force, budget)
    local now = view.time
    local hx, hy = mem.home_x, mem.home_y
    local home, ncav = {}, 0
    if not mem.out then
        for _, id in ipairs(force.units) do
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil then
                home[#home + 1] = u
                if M.fast(u.type) > 0 then
                    ncav = ncav + 1
                end
            end
        end
    end
    local riders = G.role(memory, "riders")
    if riders ~= nil then   -- last: a detachment to take a mine takes the front of the list
        for _, id in ipairs(riders.units) do
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil then
                home[#home + 1] = u
                ncav = ncav + 1
            end
        end
    end
    if view.threat_strength > 0 then
        return
    end
    local nh = #home
    local size = math.min(nh // 2, AR_DET_SIZE)
    local behind = outmatched(view) or nh < AR_DET_MIN
    local cands = drop_sites(view, hx, hy)
    d.known = #cands
    if nh < AR_HIT_MIN then
        return
    end
    local ids = {}
    for k = 1, size do
        ids[k] = home[k].id
    end
    local own = own_strength(ids)
    local t, r = nil, 0
    if not behind then   -- the nearest it beats
        for _, c in ipairs(cands) do
            if t == nil and site_for_det(d, c.x, c.y, now) then
                local foe = enemy_at(c.x, c.y, SG_FOE)
                if own * 100 >= foe * S.ar_go_pct then
                    t, r = c, foe
                end
            end
        end
    end
    d.mode = t == nil and 1 or 0
    if t == nil then   -- HIT AND RUN: the least defended of those no tower it knows covers, its cavalry
        local defs = AT.defences()
        local covered = nil
        for _, c in ipairs(cands) do
            if site_hit_ok(d, c.x, c.y, now) then
                if AT.threat_at(defs, c.x, c.y) > 0 then
                    covered = covered or c
                else
                    local foe = enemy_at(c.x, c.y, SG_FOE)
                    if t == nil or foe < r then
                        t, r = c, foe
                    end
                end
            end
        end
        if t == nil then
            if covered ~= nil then
                site_defended(d, covered.x, covered.y, now)
            end
            return
        end
        if ncav < AR_HIT_MIN then
            return
        end
        -- the cavalry: mounted archers first, then the fastest
        local cav = {}
        for _, u in ipairs(home) do
            if M.fast(u.type) > 0 then
                cav[#cav + 1] = u
            end
        end
        table.sort(cav, function(a, b)
            local ra, rb = M.fast(a.type), M.fast(b.type)
            if ra ~= rb then
                return ra > rb
            end
            if (a.speed or 0) ~= (b.speed or 0) then
                return (a.speed or 0) > (b.speed or 0)
            end
            return a.id < b.id
        end)
        size = math.min(#cav, AR_HIT_SIZE)
        ids = {}
        for k = 1, size do
            ids[k] = cav[k].id
        end
    end
    local g = G.create(memory, {task = "attack", role = "det", x = t.x, y = t.y, target = t.player, manual = true})
    G.add(memory, g, ids)
    d.units, d.hp, d.out_hp, d.dead_hp, d.foe, d.trade_n, d.dealt, d.lost = {}, {}, 0, 0, {}, 0, {}, {}
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil then
            d.units[#d.units + 1] = id
            d.hp[#d.hp + 1] = u.hp
            d.out_hp = d.out_hp + u.hp
        end
    end
    d.dest_x, d.dest_y, d.origin_x, d.origin_y, d.target_id, d.target_player = t.x, t.y, t.x, t.y, t.id, t.player
    local s = site_at(d, t.x, t.y, true)
    s.tries = s.tries + 1
    d.state, d.weak, d.site_set, d.judge_t, d.order_t, d.since = 1, 0, false, nil, nil, now
    d.hit_t, d.nocit_t, d.tgt, d.vis, d.seen_max = nil, nil, {}, {}, 0
    d.sent = d.sent + 1
    if d.mode == 1 then   -- HIT AND RUN: the mine itself
        local m = mine_near(d.origin_x, d.origin_y, 12)
        if m ~= nil then
            d.vis[1] = {x = m.x, y = m.y}
            d.dest_x, d.dest_y = m.x, m.y
        end
    end
    -- DEVIATION: no crew (towns.lua founds the town on its own)
    order_det(d, false, d.mode == 0)
    print(string.format("detach start n=%d at=%d,%d mode=%s ratio=%d home=%d cands=%d", #d.units, d.dest_x, d.dest_y,
        d.mode == 1 and "hit_and_run" or "take", r, nh, #cands))
end

-- HIT AND RUN: there - each soldier on the nearest enemy citizen it sees; out when it is over
local function det_hit(view, memory, mem, d, force, recs, mid_x, mid_y, hp)
    local now = view.time
    local there = d2(mid_x, mid_y, d.dest_x, d.dest_y) <= AR_HIT_NEAR * AR_HIT_NEAR
    local cit = {}
    for _, id in ipairs(ai.seen{enemy = true, category = "citizen", near = {mid_x, mid_y, AR_HIT_R}}) do
        local u = ai.unit(id)
        if u ~= nil then
            cit[#cit + 1] = u
        end
    end
    local why = nil
    if hp * 100 < d.out_hp * (100 - AR_HIT_LOSS) then
        why = "hurt"
    elseif not there and #cit == 0 then
        if now - d.since >= 360000 then
            why = "never_got_there"
        else
            if d.order_t == nil or now - d.order_t >= 20000 then
                d.order_t = now
                order_det(d, true, false)
            end
            return
        end
    else
        if d.hit_t == nil then
            d.hit_t, d.nocit_t, d.seen_max = now, now, 0
            print(string.format("detach hit_there riders=%d at=%d,%d mine=%d,%d citizens=%d fighters=%d", #recs, mid_x, mid_y,
                d.dest_x, d.dest_y, #cit, enemy_band(mid_x, mid_y, 10)))
        end
        if #cit > d.seen_max then
            d.seen_max = #cit
        end
        if #cit == 0 and there and now - d.nocit_t >= AR_HIT_NEXT and #d.vis < AR_HIT_MINES then   -- on to the next mine
            local m = mine_near(d.origin_x, d.origin_y, 12, d.vis)
            if m ~= nil then
                d.vis[#d.vis + 1] = {x = m.x, y = m.y}
                d.dest_x, d.dest_y, d.nocit_t, d.order_t = m.x, m.y, now, now
                order_det(d, false, false)
                return
            end
        end
        if #cit > 0 then
            d.nocit_t = now
        end
        if now - d.hit_t >= AR_HIT_TIME then
            why = "time_is_up"
        elseif now - d.nocit_t >= 20000 then
            why = "nobody_left"
        elseif enemy_band(mid_x, mid_y, 10) >= #recs then
            why = "soldiers_coming"
        end
    end
    if why ~= nil then
        local killed = 0
        for _, id in ipairs(d.tgt) do
            if ai.unit(id) == nil then   -- dead or out of sight (a count for the log only)
                killed = killed + 1
            end
        end
        d.killed = d.killed + killed
        print(string.format("detach hit_over attacked=%d killed=%d seen_max=%d there_s=%d", #d.tgt, killed, d.seen_max,
            d.hit_t ~= nil and (now - d.hit_t) // 1000 or 0))
        det_end(view, memory, mem, d, force, "hit_and_run_" .. why, false)
        return
    end
    if #cit == 0 then
        return
    end
    local by = {}
    for _, u in ipairs(recs) do
        if u.action ~= "attack" then
            local best, bd = nil, nil
            for _, c in ipairs(cit) do
                local q = d2(c.x, c.y, u.x, u.y)
                if bd == nil or q < bd then
                    best, bd = c, q
                end
            end
            by[best.id] = by[best.id] or {}
            by[best.id][#by[best.id] + 1] = u.id
        end
    end
    for _, tid in ipairs(ai.keys(by)) do
        if ai.attack(by[tid], tid) == "ok" then
            local seen = false
            for _, id in ipairs(d.tgt) do
                if id == tid then
                    seen = true
                end
            end
            if not seen and #d.tgt < 32 then
                d.tgt[#d.tgt + 1] = tid
            end
        end
    end
end

-- the target building still stands: its record in the memory (a building stays until seen gone)
local function target_alive(d)
    for _, m in ipairs(ai.memory{buildings = true, near = {d.dest_x, d.dest_y, 2}}) do
        if m.id == d.target_id then
            return true
        end
    end
    return false
end

-- the nearest enemy building known within r of a tile (not a wall)
local function near_building(x, y, r)
    for _, m in ipairs(ai.memory{buildings = true, near = {x, y, r}, limit = 8}) do
        if not AT.is_wall(m.type) then
            return m
        end
    end
    return nil
end

-- the town over the site secured: an own finished drop site and two towers within 18 tiles (DEVIATION: upstream's
-- claim town; here towns.lua's own founding is waited for)
local function secured(x, y)
    local drop, towers = 0, 0
    for _, id in ipairs(ai.units{category = "building", built = true, near = {x, y, 18}}) do
        local u = ai.unit(id)
        if u ~= nil then
            if DROPS[u.type] then
                drop = drop + 1
            elseif u.armed then
                towers = towers + 1
            end
        end
    end
    return drop > 0 and towers >= 2
end

local function det_tick(view, memory, mem, d, force, order, budget)
    local now = view.time
    riders_tick(view, memory, mem, d, force, order, budget)
    look_tick(view, memory, mem, d, force, budget)
    if d.state == 0 then
        if (d.last ~= nil and now - d.last < ((d.mode_last or 0) == 1 and AR_HIT_PAUSE or AR_DET_PAUSE))
                or (d.t ~= nil and now - d.t < AR_DET_LOOK) then
            return
        end
        d.t = now
        det_start(view, memory, mem, d, force, budget)
        return
    end
    local recs, mid_x, mid_y, hp = members(d)
    local m = #recs
    if m < (d.mode == 1 and 2 or 4) then
        d.losing = d.losing + 1
        det_end(view, memory, mem, d, force, "too_few_left", true)
        return
    end
    if d.mode == 1 then
        det_hit(view, memory, mem, d, force, recs, mid_x, mid_y, hp)
        return
    end
    local own = own_strength(d.units)
    if d.judge_t == nil or now - d.judge_t >= S.ar_judge_every then
        d.judge_t = now
        local foe = math.max(enemy_at(mid_x, mid_y, SG_FOE), enemy_at(d.dest_x, d.dest_y, SG_FOE))
        d.foe_hp = foe
        local fighting, dealt, lost = trade(d, recs, mid_x, mid_y)
        if fighting >= AR_CONTACT_MIN and fighting * 10 >= m then   -- the fight judges
            local k = d.trade_n % AR_TRADE_N + 1
            d.dealt[k], d.lost[k] = dealt, lost
            d.trade_n = d.trade_n + 1
            local tn = math.min(d.trade_n, AR_TRADE_N)
            local sd, sl = 0, 0
            for i = 1, tn do
                sd, sl = sd + (d.dealt[i] or 0), sl + (d.lost[i] or 0)
            end
            d.weak = 0
            if tn >= AR_TRADE_MIN and sl * 100 > sd * AR_TRADE_BAD and sl * 100 >= d.out_hp * AR_DET_TRADE_FLOOR then
                d.losing = d.losing + 1
                det_end(view, memory, mem, d, force, "trading_badly", true)
                return
            end
        elseif foe > 0 and own * 100 < foe * AR_FAR_PCT then
            d.weak = d.weak + 1
            if d.weak >= 2 then
                d.losing = d.losing + 1
                det_end(view, memory, mem, d, force, "far_too_weak", true)
                return
            end
        else
            d.weak = 0
        end
    end
    if d.state == 2 then
        if secured(d.site_x, d.site_y) then
            d.secured = d.secured + 1
            det_end(view, memory, mem, d, force, "mine_secured", false)
            return
        end
        if now - d.claim_t >= AR_DET_HOLD then
            d.timed_out = d.timed_out + 1
            det_end(view, memory, mem, d, force, "held_15_min", false)
            return
        end
        if d.order_t == nil or now - d.order_t >= 20000 then
            d.order_t = now
            order_det(d, true, true)
        end
        return
    end
    if target_alive(d) then
        if d.order_t == nil or now - d.order_t >= 20000 then
            d.order_t = now
            order_det(d, true, true)
        end
        return
    end
    if not d.site_set then
        local mn = mine_near(d.dest_x, d.dest_y, 12)
        if mn ~= nil then
            d.site_x, d.site_y, d.site_set = d.dest_x, d.dest_y, true
        end
    end
    if not d.site_set then
        det_end(view, memory, mem, d, force, "no_mine_there", false)
        return
    end
    local nb = near_building(d.site_x, d.site_y, 22)
    if nb ~= nil then
        local foe = enemy_at(nb.x, nb.y, SG_FOE)
        if own * 100 < foe * S.ar_go_pct then
            d.no_town = d.no_town + 1
            det_end(view, memory, mem, d, force, "left_too_strong", true)
            return
        end
        d.dest_x, d.dest_y, d.target_id, d.order_t = nb.x, nb.y, nb.id, now
        d.cleared = d.cleared + 1
        order_det(d, false, true)
        return
    end
    -- DEVIATION: upstream founds the town (tn_claim, the crew); here the detachment holds the site for towns.lua's
    d.state, d.dest_x, d.dest_y, d.claim_t, d.order_t = 2, d.site_x, d.site_y, now, nil
    d.founded = d.founded + 1
    print(string.format("detach hold n=%d at=%d,%d", #d.units, d.site_x, d.site_y))
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (after the force's)

function M.step(view, memory, mem, force, order, budget)
    if not S.mine_raids or force == nil or view.town_x < 0 or mem.home_x == nil then
        return
    end
    local d = st(mem)
    det_tick(view, memory, mem, d, force, order, budget)
    if view.time % 60000 < 1000 then
        local riders = G.role(memory, "riders")
        print(string.format("detach state=%d mode=%d n=%d sent=%d cleared=%d held=%d secured=%d losing=%d no_town=%d timed=%d killed=%d trained=%d riders=%d known=%d sites=%d",
            d.state, d.mode, #d.units, d.sent, d.cleared, d.founded, d.secured, d.losing, d.no_town, d.timed_out, d.killed,
            d.trained, riders ~= nil and #riders.units or 0, d.known, #d.sites))
    end
end

return M

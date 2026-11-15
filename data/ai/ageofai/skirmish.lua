-- Age of AI port, THE SKIRMISHERS (upstream dll/ai/skirmish.c, SK_ON: off until measured; UPSTREAM has the map and
-- the licence: GPL-3.0, Grille and the Age of AI contributors). require("ageofai.skirmish"), called from army.lua's
-- step after the force; settings.lua `skirmish` (bits: 1 the party, 2 the spotter) switches it on.
--
-- The player, 2026-11-04, on his save "choke wolf" (a narrow valley his artillery and a few units at its end cover;
-- the AI's force burned itself in it): "be tactical, not commit the whole army into it, sharpshooters are a good way
-- to get close and pick off targets before the enemy can react, or even force the enemy to break formation to hunt
-- the sharpshooters, which could give an opening to other units". Every SK_EVERY with the force at home:
-- - WHEN: a position holds the force's way - the shooters' spot (where seen land units hit the AI's from AF_SHOOT_D
--   tiles or more, the spot hit from most often; airstrike.c's as_shooters, kept here from the attacked events) or
--   the target the last attack time found too strong (army.lua's mem.strong) -, within SK_REACH of home, SK_REST
--   after the last party came home.
-- - WHO: up to SK_N of the force's members at home that shoot SK_RANGE tiles or further or are field guns from
--   AF_GUN_D, the longest first, with SK_FIT of their hit points; SK_MIN at least, else nobody goes. They leave the
--   force for the sortie (none of its orders reach them: a manual group); home again they join the force.
-- - THE SORTIE: an attack-move to the stand SK_STAND tiles short of the position on the line from home; on the way
--   and there each member not attacking already goes for a long-range shooter seen within SK_HUNT - close in on what
--   outranges it -, else an enemy soldier it sees within its range + SK_PICK (no buildings); one idle short of the
--   stand walks on. Nothing to shoot at the stand: it creeps SK_CREEP closer every SK_CREEP_MS, to SK_STAND_MIN.
-- - THE PULL-BACK: enemy fighters coming at it - seen within SK_CHASE_R of the party's middle, moved SK_MOVED tiles
--   or more since the last look (a defender standing its ground chases nobody) - that it doesn't beat by SK_HOLD_PCT,
--   its hit points under SK_BLEED_PCT of what it went out with, or SK_MAX_MS out: home, a plain move.
-- - THE BAIT: the party on its way home with chasers seen within SK_CHASE_R of it, and they within SK_AMBUSH of home:
--   the force at home goes at them as at a raid (army.lua's raid answer, mem.sk.bait) when it beats them by
--   ar_go_pct - the enemy broke formation to hunt the party, the force fights it on its own ground.
-- THE SPOTTER (bit 2; the player: "sharpshooters are great scouts, if set to passive it's completely invisible to the
-- enemy if it doesn't get too close"): while there is such a position, one sharpshooter or sniper of the force - one
-- trained when the force has none, every SP_TRAIN_MS - leaves it, takes the Scout stance (it never attacks) and
-- walks to the point SP_BACK inside its sight short of the position on the line from home, and stands there: what it
-- sees the party picks and the force's judgement counts. The position gone, it gets its stance back and joins the
-- force. Counter-fire leaves it alone (tactics.lua).
-- Measured upstream on the save and on the wall test: level on all counts; off. Not ported: the "revealed" shooters
-- (intel.c: an unseen unit hitting the AI's; the engine's attacker field names only a seen attacker), the air force's
-- shooters' list (the events stand in), the ratio from the game's damage tables (st_ratio: hit points x attack here).
local G = require("lib.groups")
local S = require("ageofai.settings")
local M = {}

local SK_EVERY, SK_N, SK_MIN, SK_RANGE, SK_FIT, SK_REACH, SK_REST = 3000, 8, 3, 700, 80, 80, 30000
local SK_STAND, SK_STAND_MIN, SK_CREEP, SK_CREEP_MS, SK_PICK, SK_HUNT = 10, 5, 3, 15000, 2, 14
local SK_CHASE_R, SK_MOVED, SK_HOLD_PCT, SK_BLEED_PCT, SK_MAX_MS, SK_HOME_MS, SK_AMBUSH = 10, 2, 150, 50, 240000, 120000, 25
local SP_BACK, SP_TRAIN_MS, SP_ORDER_MS = 3, 60000, 20000
local AF_SHOOT_D, AF_GUN_D, AF_SHOOT_MS, AF_SHOOT_JOIN, AF_SHOOTS, AF_STRONG_MS = 7, 5, 180000, 5, 16, 300000
local SPOTTERS = {inf10_sniper = true, inf08_sharpshooter = true}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function isqrt(v)
    if v <= 0 then
        return 0
    end
    local r = math.floor(math.sqrt(v))
    while r * r > v do
        r = r - 1
    end
    while (r + 1) * (r + 1) <= v do
        r = r + 1
    end
    return r
end

local function st(mem)
    mem.sk = mem.sk or {state = 0, units = {}, shoots = {}, near = {}, sorties = 0, picks = 0, chased = 0, bled = 0,
                        timed = 0, baits = 0, members = 0, lost = 0, spotters = 0, sp_lost = 0, sp_trained = 0}
    return mem.sk
end

-- a field gun: siege, artillery, the medieval field weapons, the cannons (sh_gun; by the class name)
function M.gun(t)
    t = t or ""
    local p = string.sub(t, 1, 3)
    return p == "gun" or string.sub(t, 1, 5) == "siege" or string.find(t, "mortar", 1, true) ~= nil
        or string.find(t, "artillery", 1, true) ~= nil
end

-- THE SHOOTERS' SPOTS (airstrike.c as_shooters): a seen land unit that hit one of ours from AF_SHOOT_D tiles or more
-- (a gun from AF_GUN_D), joined into spots within AF_SHOOT_JOIN; the step's attacked events
function M.note_hits(mem, events, now)
    if (S.skirmish or 0) == 0 then
        return
    end
    local s = st(mem)
    for _, e in ipairs(events or {}) do
        if e.kind == "attacked" and e.attacker ~= nil then
            local a = ai.unit(e.attacker)
            local v = ai.unit(e.unit)
            if a ~= nil and v ~= nil and a.category ~= "building" and a.category ~= "ship" and a.category ~= "air"
                    and a.category ~= "boat" then
                local dist = isqrt(d2(a.x, a.y, v.x, v.y))
                if dist >= (M.gun(a.type) and AF_GUN_D or AF_SHOOT_D) then
                    local j, old = nil, 1
                    for k, sp in ipairs(s.shoots) do
                        if j == nil and now - sp.t < AF_SHOOT_MS and d2(sp.x, sp.y, a.x, a.y) <= AF_SHOOT_JOIN * AF_SHOOT_JOIN then
                            j = k
                        end
                        if sp.t < s.shoots[old].t then
                            old = k
                        end
                    end
                    if j == nil then
                        if #s.shoots < AF_SHOOTS then
                            j = #s.shoots + 1
                        else
                            j = old
                        end
                        s.shoots[j] = {x = a.x, y = a.y, hits = 0, t = now}
                    end
                    s.shoots[j].hits = s.shoots[j].hits + 1
                    s.shoots[j].t = now
                end
            end
        end
    end
end

-- the position the force can't take: the shooters' spot (1), else the target found too strong (2); nil none
local function position(mem, s, now)
    local best = nil
    for _, sp in ipairs(s.shoots) do
        if now - sp.t < AF_SHOOT_MS and (best == nil or sp.hits > best.hits) then
            best = sp
        end
    end
    if best ~= nil then
        return best.x, best.y, 1
    end
    if mem.strong_t ~= nil and now - mem.strong_t < AF_STRONG_MS then
        return mem.strong_x, mem.strong_y, 2
    end
    return nil
end

-- the party's living members, their middle and hit points
local function side(s)
    local recs, sx, sy, n, hp = {}, 0, 0, 0, 0
    local keep = {}
    for _, id in ipairs(s.units) do
        local u = ai.unit(id)
        if u ~= nil then
            keep[#keep + 1] = id
            recs[#recs + 1] = u
            sx, sy, n, hp = sx + u.x, sy + u.y, n + 1, hp + u.hp
        end
    end
    s.units = keep
    if n == 0 then
        return recs, nil, nil, 0
    end
    return recs, sx // n, sy // n, hp
end

-- enemy land fighters seen within r of (x, y) that moved SK_MOVED since the last look (SK_EVERY ago): their count,
-- strength and middle
local function chasers(s, x, y, r, now)
    local fresh = s.near_t ~= nil and now - s.near_t <= 2 * SK_EVERY
    local seen, n, str, sx, sy = {}, 0, 0, 0, 0
    for _, id in ipairs(ai.seen{enemy = true, armed = true, category = "soldier", near = {x, y, r}}) do
        local u = ai.unit(id)
        if u ~= nil then
            seen[#seen + 1] = {id = id, x = u.x, y = u.y}
            local last = nil
            for _, o in ipairs(s.near) do
                if o.id == id then
                    last = o
                end
            end
            if fresh and last ~= nil and d2(last.x, last.y, u.x, u.y) >= SK_MOVED * SK_MOVED then
                n, str, sx, sy = n + 1, str + u.hp * (u.attack or 0), sx + u.x, sy + u.y
            end
        end
    end
    s.near, s.near_t = seen, now
    if n == 0 then
        return 0, 0, x, y
    end
    return n, str, sx // n, sy // n
end

-- the stand (or the spotter's point): d tiles short of the position towards home
local function short_of(px, py, hx, hy, d)
    local dx, dy = hx - px, hy - py
    local l = isqrt(dx * dx + dy * dy)
    if l <= d then
        return hx, hy
    end
    return px + dx * d // l, py + dy * d // l
end

local function range_of(u)
    return (u.range or 0)
end

local function go(view, memory, mem, s, force)
    local now = view.time
    local px, py, why = position(mem, s, now)
    local hx, hy = mem.home_x, mem.home_y
    if why == nil or d2(px, py, hx, hy) > SK_REACH * SK_REACH then
        return
    end
    -- the longest range first
    local c = {}
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and d2(u.x, u.y, hx, hy) <= 4 * S.ar_home_r * S.ar_home_r
                and u.hp * 100 >= (u.hp_max or u.hp) * SK_FIT then
            local rg = range_of(u)
            if rg >= SK_RANGE or (M.gun(u.type) and rg >= AF_GUN_D * 100) then
                c[#c + 1] = {id = id, rg = rg}
            end
        end
    end
    if #c < SK_MIN then
        return
    end
    table.sort(c, function(a, b)
        if a.rg ~= b.rg then
            return a.rg > b.rg
        end
        return a.id < b.id
    end)
    local ids = {}
    for k = 1, math.min(#c, SK_N) do
        ids[k] = c[k].id
    end
    local g = G.create(memory, {task = "attack", role = "skirmish", x = px, y = py, manual = true})
    G.add(memory, g, ids)
    s.units = ids
    s.pos_x, s.pos_y = px, py
    s.stand_x, s.stand_y = short_of(px, py, hx, hy, SK_STAND)
    s.state, s.out_t, s.creep_t = 1, now, now
    local _, _, _, hp = side(s)
    s.out_hp = hp
    s.sorties = s.sorties + 1
    s.members = s.members + #ids
    ai.move(ids, s.stand_x, s.stand_y, {forced = false})
    print(string.format("skirmish party n=%d range=%d-%d hp=%d why=%s at=%d,%d stand=%d,%d", #ids, c[#ids].rg // 100,
        c[1].rg // 100, hp, why == 1 and "shooters" or "strong", px, py, s.stand_x, s.stand_y))
end

-- a member's target: a long-range shooter seen within SK_HUNT, else a soldier seen within its range + SK_PICK
local function pick(s, u)
    local r = range_of(u) // 100 + SK_PICK
    local h = math.max(r, SK_HUNT)
    local best, bd = nil, nil
    for _, id in ipairs(ai.seen{enemy = true, category = "soldier", near = {u.x, u.y, h}}) do
        local e = ai.unit(id)
        if e ~= nil then
            local d = d2(e.x, e.y, u.x, u.y)
            if M.gun(e.type) or (e.range or 0) >= AF_SHOOT_D * 100 then
                -- the shooters first
            elseif d <= r * r then
                d = d + h * h
            else
                d = nil
            end
            if d ~= nil and (bd == nil or d < bd) then
                best, bd = e, d
            end
        end
    end
    if best == nil then
        return false
    end
    if ai.attack({u.id}, best.id) ~= "ok" then
        return false
    end
    s.picks = s.picks + 1
    return true
end

-- the members back into the force (as soldiers at home: its gather walks them there)
local function disband(memory, s, force)
    local g = G.role(memory, "skirmish")
    for _, id in ipairs(s.units) do
        if ai.unit(id) == nil then
            s.lost = s.lost + 1
        end
    end
    if g ~= nil then
        if #g.units > 0 then
            G.add(memory, force, g.units)
        end
        G.disband(memory, g)
    end
    s.units, s.state = {}, 0
end

local function spotter(view, memory, mem, s, force, order)
    local now = view.time
    local hx, hy = mem.home_x, mem.home_y
    local sp = G.role(memory, "spotter")
    if sp ~= nil and #sp.units == 0 then
        print(string.format("skirmish spotter_lost at=%d,%d", s.sp_x or -1, s.sp_y or -1))
        G.disband(memory, sp)
        sp = nil
        s.sp_lost = s.sp_lost + 1
        mem.sk_spotter = nil
    end
    local px, py, why = position(mem, s, now)
    if why ~= nil and d2(px, py, hx, hy) > SK_REACH * SK_REACH then
        why = nil
    end
    if why == nil then   -- no position: back into the force
        if sp ~= nil then
            local id = sp.units[1]
            if s.sp_stance ~= nil and s.sp_stance >= 0 then
                ai.stance({id}, s.sp_stance)
            end
            G.add(memory, force, {id})
            G.disband(memory, sp)
            mem.sk_spotter = nil
            print("skirmish spotter_back")
        end
        return
    end
    if sp == nil then   -- one of the force's, the nearest home
        local best, bd = nil, nil
        for _, id in ipairs(force.units) do
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil and SPOTTERS[u.type] then
                local d = d2(u.x, u.y, hx, hy)
                if bd == nil or d < bd then
                    best, bd = u, d
                end
            end
        end
        if best == nil then   -- none: one trained
            if s.sp_train_t ~= nil and now - s.sp_train_t < SP_TRAIN_MS then
                return
            end
            s.sp_train_t = now
            for _, t in ipairs({"inf10_sniper", "inf08_sharpshooter"}) do
                for _, c in ipairs(view.costs) do
                    if c.type == t and c.producer ~= nil and c.producers > 0 then
                        order({verb = "train", type = t, n = 1})
                        s.sp_trained = s.sp_trained + 1
                        print(string.format("skirmish spotter_train type=%s", t))
                        return
                    end
                end
            end
            return
        end
        sp = G.create(memory, {task = "defend", role = "spotter", x = px, y = py, radius = 0, manual = true})
        G.add(memory, sp, {best.id})
        s.sp_stance = best.stance or -1
        ai.stance({best.id}, 0)   -- Scout: never attacks
        s.sp_t = nil
        s.spotters = s.spotters + 1
        mem.sk_spotter = best.id
    end
    local id = sp.units[1]
    local u = ai.unit(id)
    if u == nil then
        return
    end
    -- its point: SP_BACK inside its sight short of the position
    local sight = (u.sight or 800) // 100
    local d = math.max(sight - SP_BACK, 4)
    local ax, ay = short_of(px, py, hx, hy, d)
    if s.sp_x == nil or d2(ax, ay, s.sp_x, s.sp_y) > 4 or s.sp_t == nil then
        print(string.format("skirmish spotter type=%s sight=%d to=%d,%d why=%s at=%d,%d", u.type, sight, ax, ay,
            why == 1 and "shooters" or "strong", px, py))
    end
    s.sp_x, s.sp_y = ax, ay
    if d2(u.x, u.y, ax, ay) <= 2 or (u.action ~= "idle" and s.sp_t ~= nil and now - s.sp_t < SP_ORDER_MS) then
        return
    end
    if ai.move({id}, ax, ay, {forced = true}) == "ok" then
        s.sp_t = now
    end
end

function M.step(view, memory, mem, force, order)
    local on = S.skirmish or 0
    if on == 0 or force == nil or view.town_x < 0 or mem.home_x == nil then
        return
    end
    local s = st(mem)
    local now = view.time
    if s.t ~= nil and now - s.t < SK_EVERY then
        return
    end
    s.t = now
    if (s.sorties > 0 or s.state ~= 0 or s.spotters > 0 or s.sp_trained > 0) and (s.log_t == nil or now - s.log_t >= 60000) then
        s.log_t = now
        print(string.format("skirmish sorties=%d members=%d lost=%d picks=%d chased=%d bled=%d timed=%d baits=%d out=%s spotters=%d sp_lost=%d sp_trained=%d",
            s.sorties, s.members, s.lost, s.picks, s.chased, s.bled, s.timed, s.baits, tostring(s.state ~= 0), s.spotters,
            s.sp_lost, s.sp_trained))
    end
    if (on & 2) ~= 0 then
        spotter(view, memory, mem, s, force, order)
    end
    if (on & 1) == 0 then
        return
    end
    local hx, hy = mem.home_x, mem.home_y
    if s.state == 0 then
        if not mem.out and (s.end_t == nil or now - s.end_t >= SK_REST) then
            go(view, memory, mem, s, force)
        end
        return
    end
    local recs, mid_x, mid_y, hp = side(s)
    local alive = #recs
    if mid_x == nil then
        mid_x, mid_y = hx, hy
    end
    if alive < SK_MIN and s.state == 1 then
        print(string.format("skirmish down n=%d home", alive))
        s.state = 2
        s.bled = s.bled + 1
        if alive > 0 then
            ai.move(s.units, hx, hy, {forced = true})
        end
    end
    local nch, foe, cmx, cmy = chasers(s, mid_x, mid_y, SK_CHASE_R, now)
    local own = ai.strength(s.units)
    if s.state == 1 then
        local why = nil
        if nch > 0 and own * 100 < foe * SK_HOLD_PCT then
            why, s.chased = "chased", s.chased + 1
        elseif hp * 100 < s.out_hp * SK_BLEED_PCT then
            why, s.bled = "bled", s.bled + 1
        elseif now - s.out_t >= SK_MAX_MS then
            why, s.timed = "time_up", s.timed + 1
        end
        if why ~= nil then
            print(string.format("skirmish home n=%d at=%d,%d why=%s hp=%d/%d chasers=%d own=%d foe=%d", alive, mid_x, mid_y,
                why, hp, s.out_hp, nch, own, foe))
            s.state, s.out_t = 2, now
            ai.move(s.units, hx, hy, {forced = true})
            return
        end
        local at_stand = d2(mid_x, mid_y, s.stand_x, s.stand_y) <= 4 * S.ar_done * S.ar_done
        local picked, walk = 0, {}
        for _, u in ipairs(recs) do
            if u.action ~= "attack" then
                if pick(s, u) then
                    picked = picked + 1
                elseif u.action == "idle" and not at_stand then
                    walk[#walk + 1] = u.id   -- idle short of the stand: on
                end
            end
        end
        if #walk > 0 then
            ai.move(walk, s.stand_x, s.stand_y, {forced = false})
        end
        if at_stand and picked == 0 and nch == 0 and now - s.creep_t >= SK_CREEP_MS
                and d2(s.stand_x, s.stand_y, s.pos_x, s.pos_y) > SK_STAND_MIN * SK_STAND_MIN then   -- nothing to shoot: closer
            s.creep_t = now
            local dx, dy = s.pos_x - s.stand_x, s.pos_y - s.stand_y
            local l = isqrt(dx * dx + dy * dy)
            if l > 0 then
                s.stand_x, s.stand_y = s.stand_x + dx * SK_CREEP // l, s.stand_y + dy * SK_CREEP // l
            end
            local idle = {}
            for _, u in ipairs(recs) do
                if u.action == "idle" then
                    idle[#idle + 1] = u.id
                end
            end
            if #idle > 0 then
                ai.move(idle, s.stand_x, s.stand_y, {forced = false})
            end
        end
        return
    end
    -- home bound: the chasers near home - the force goes at them (army.lua's raid answer takes mem.sk.bait)
    if nch > 0 and not mem.out and d2(cmx, cmy, hx, hy) <= SK_AMBUSH * SK_AMBUSH then
        local str = force.strength or 0
        local there = 0
        for _, m in ipairs(ai.memory{near = {cmx, cmy, 12}}) do
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                there = there + m.hp * c.attack
            end
        end
        if str * 100 >= there * S.ar_go_pct then
            s.baits = s.baits + 1
            s.bait = {x = cmx, y = cmy, n = nch, t = now}
            print(string.format("skirmish bait chasers=%d at=%d,%d force=%d foe=%d", nch, cmx, cmy, str, there))
        end
    end
    if d2(mid_x, mid_y, hx, hy) <= 4 * S.ar_home_r * S.ar_home_r or alive == 0 or now - s.out_t >= SK_HOME_MS then
        print(string.format("skirmish party_home n=%d of=%d hp=%d/%d", alive, s.members, hp, s.out_hp))
        disband(memory, s, force)
        s.end_t = now
    end
end

-- army.lua: the bait to answer as a raid (once), or nil
function M.bait(mem, now)
    local s = mem.sk
    if s == nil or s.bait == nil then
        return nil
    end
    local b = s.bait
    s.bait = nil
    if now - b.t <= 10000 then
        return b
    end
    return nil
end

return M

-- The home guard over the per-unit interface (docs/AI_LUA_UNITS.md §3; docs/AI_RESEARCH.md §8.39).
-- require("lib.guard"). Generic: any script may use it; the numbers are options.
--
--   GD.step(view, mem, opts)   every strategy step: the free soldiers join the guard (a manual lib.groups group of
--                              role "guard"), which waits at its post under a tower, fights the raids in the town's
--                              zone with focus fire and pulls its hurt units back. opts: zone (tiles round the town
--                              centre it fights in; default the claim range)
--   GD.mix(view, mem)          the shares of the guard by role (pierce, ranged, shock) for the raiders seen
--   GD.train(view, mem, n, order, per)
--                              trains towards a guard of n by the mix; `order(tag, o)` is the script's order function
--   GD.group(mem)              the guard's group (lib.groups), or nil
--
-- The rules and where they come from:
-- - [owner, research §4.3 step 6] the Stone Age guard waits at home under the towers: its post is the own tower
--   nearest the citizens' centre (the worked sites), else the town centre.
-- - [research §1.6, §4.2 item 3] the mix: about two-thirds spearmen and one-third slingers; by the raiders seen
--   (ai.memory, fog-honest): spearmen against shock infantry and cavalry, slingers against spearmen, clubmen
--   against slingers and rock throwers.
-- - [owner, research §2.7] focus fire: the raider nearest the citizens first, the healthy units in turn on the
--   next once enough fire is on one; units already on a valid target keep it.
-- - [owner] no chase past the zone: a raid outside the town's zone (the claim range round the town centre, or 10
--   tiles round an own building) is left alone, and the guard goes back to its post.
-- - [balanced: military.militia_towers; research §4.3 step 3] it engages at `engage_odds` of the raid counting the
--   towers whose reach the raid stands in, and always under a tower; else it holds its post.
-- - [research §2.7] a unit under `hurt_pct` of its hit points pulls back to the post (or the town centre) and stays
--   out of the fight.
local G = require("lib.groups")
local D = require("lib.defence")

local M = {}

M.opts = {
    engage_odds = 80,     -- percent: the guard and the towers in reach against the raid (Lanchester)
    hurt_pct = 35,        -- a unit under this share of its hit points pulls back
    post_ms = 30000,      -- the post is chosen again this often
    home_r = 6,           -- units further than this from the post walk back while nothing is fought
    building_r = 10,      -- a raid this near an own building is in the zone
    focus_hits = 3,       -- a target takes units until their attack x this reaches its hit points
    deny = true,          -- the enemy citizens in the zone are hunted while no raid is on ...
    deny_n = 4,           -- ... at most this many at once
}

-- the roles' classes, the newest first (the first the player can train now is taken)
M.ROLES = {
    pierce = {"inf06_pikeman", "inf04_phalanx", "inf02_spearman"},
    ranged = {"arch05_composite_bow", "arch03_simple_bowman", "arch02_slinger", "inf01_rock_thrower"},
    shock = {"inf04_short_sword", "inf03_maceman", "inf01_clubman"},
}
M.ROLE_ORDER = {"pierce", "ranged", "shock"}

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

-- the role of a soldier class by its name (and range): ranged, pierce, shock or cavalry
local function role_of(t, range)
    if t == nil then
        return "shock"
    end
    if string.find(t, "^cav") or string.find(t, "chariot") then
        return "cavalry"
    end
    if (range or 0) >= 150 or string.find(t, "^arch") or string.find(t, "sling") or string.find(t, "rock_thrower")
            or string.find(t, "javelin") or string.find(t, "pilum") then
        return "ranged"
    end
    if string.find(t, "spear") or string.find(t, "phalanx") or string.find(t, "pike") or string.find(t, "halb") then
        return "pierce"
    end
    return "shock"
end
M.role_of = role_of

function M.group(mem)
    return G.role(mem, "guard")
end

-- ---------------------------------------------------------------------------------------------------------------
-- The mix and the training

-- the raiders seen in the last memory_ms (ai.memory: what the player saw) by role -> counts, total
local function seen_roles(mem, now)
    if mem.guard_seen ~= nil and now - (mem.guard_seen.t or 0) < 30000 then
        return mem.guard_seen
    end
    local s = {ranged = 0, pierce = 0, shock = 0, cavalry = 0, n = 0, t = now}
    for _, m in ipairs(ai.memory{enemy = true, category = "soldier", limit = 200}) do
        local c = ai.class(m.type)
        local r = role_of(m.type, c ~= nil and c.range or 0)
        s[r] = s[r] + 1
        s.n = s.n + 1
    end
    -- the raids that came are remembered longer (a raid of slingers two waves ago still says slingers)
    mem.guard_raids = mem.guard_raids or {ranged = 0, pierce = 0, shock = 0, cavalry = 0}
    mem.guard_seen = s
    return s
end

-- the guard's shares (percent) by role
function M.mix(view, mem)
    local s = seen_roles(mem, view.time)
    local r = mem.guard_raids or {ranged = 0, pierce = 0, shock = 0, cavalry = 0}
    local ranged, pierce, shock, cav = s.ranged + r.ranged, s.pierce + r.pierce, s.shock + r.shock, s.cavalry + r.cavalry
    local t = ranged + pierce + shock + cav
    -- [research §4.2 item 3] 2/3 spearmen, 1/3 slingers with nothing seen; the counters by what was seen
    local w = {pierce = 200, ranged = 100, shock = 0}
    if t > 0 then
        w.pierce = w.pierce + 300 * (shock + cav) // t
        w.ranged = w.ranged + 300 * pierce // t
        w.shock = w.shock + 300 * ranged // t
    end
    local sum = w.pierce + w.ranged + w.shock
    return {pierce = w.pierce * 100 // sum, ranged = w.ranged * 100 // sum, shock = w.shock * 100 // sum}
end

-- the class of a role the player can train now (view.costs, a producer built) -> the cost entry, or nil
local function class_for(view, role)
    for _, t in ipairs(M.ROLES[role]) do
        for _, c in ipairs(view.costs) do
            if c.type == t and not c.building and c.military and c.producers > 0 then
                return c
            end
        end
    end
    return nil
end

-- trains towards a guard of n soldiers by the mix: each role's lack from its producer, at most `per` an order, one
-- order a class at a time (as boom's own training)
function M.train(view, mem, n, order, per)
    local have = {pierce = 0, ranged = 0, shock = 0, cavalry = 0}
    local total = 0
    for _, s in ipairs(view.soldiers or {}) do
        local r = role_of(s.type, 0)
        have[r] = (have[r] or 0) + s.count
        total = total + s.count
    end
    for _, c in ipairs(view.costs) do
        if not c.building and c.military then
            local q = c.requested + c.in_production
            if q > 0 then
                local r = role_of(c.type, 0)
                have[r] = (have[r] or 0) + q
                total = total + q
            end
        end
    end
    local need = n - total
    if need <= 0 then
        return
    end
    local mix = M.mix(view, mem)
    -- each role's lack, the largest first; a role with no class to train gives its share to pierce
    local lack = {}
    local spill = 0
    for _, r in ipairs(M.ROLE_ORDER) do
        local want = (n * mix[r] + 50) // 100
        local c = class_for(view, r)
        if c == nil then
            spill = spill + math.max(want - have[r], 0)
        else
            lack[#lack + 1] = {role = r, n = math.max(want - have[r], 0), c = c}
        end
    end
    for _, l in ipairs(lack) do
        if l.role == "pierce" or (spill > 0 and l == lack[1]) then
            l.n = l.n + spill
            spill = 0
        end
    end
    table.sort(lack, function(a, b)
        if a.n ~= b.n then
            return a.n > b.n
        end
        return a.role < b.role
    end)
    local busy = {}
    for _, l in ipairs(lack) do
        local k = math.min(l.n, need, per or 2)
        if k > 0 and l.c.requested == 0 and not busy[l.c.producer] then
            order("soldier_" .. l.c.producer, {verb = "train", type = l.c.type, n = k})
            busy[l.c.producer] = true
            need = need - k
        end
    end
end

-- [balanced: military.standing_army 0.25 a worker, army_floor] a plan whose light army grows with the citizens:
-- plan.army_per (percent of the citizens), at least plan.army_floor, at most plan.soldiers_max -> a copy of the plan
function M.scaled(plan, citizens)
    local n = math.max(plan.army_floor or 0, citizens * plan.army_per // 100)
    local p = {}
    for k, v in pairs(plan) do   -- (a copy: the order does not matter)
        p[k] = v
    end
    p.soldiers = math.min(n, plan.soldiers_max)
    return p
end

-- ---------------------------------------------------------------------------------------------------------------
-- The guard's step

-- the post: the own built tower nearest the citizens' centre, else the town centre (every post_ms)
local function choose_post(view, mem, g)
    local now = view.time
    if g.post_t ~= nil and now - g.post_t < M.opts.post_ms and g.post_x ~= nil then
        return
    end
    g.post_t = now
    local cits = ai.units{category = "citizen", near = {view.town_x, view.town_y, view.claim_range + 5}, limit = 80}
    local cx, cy = view.town_x, view.town_y
    if #cits > 0 then
        local c = ai.get(cits, {"x", "y"})
        local sx, sy, n = 0, 0, 0
        for k = 1, #cits do
            if c.x[k] ~= nil then
                sx, sy, n = sx + c.x[k], sy + c.y[k], n + 1
            end
        end
        if n > 0 then
            cx, cy = sx // n, sy // n
        end
    end
    local px, py, best = view.town_x, view.town_y, d2(view.town_x, view.town_y, cx, cy)
    local towers = ai.units{category = "building", armed = true, built = true,
                            near = {view.town_x, view.town_y, view.claim_range + 5}, limit = 16}
    if #towers > 0 then
        local c = ai.get(towers, {"x", "y"})
        for k = 1, #towers do
            if c.x[k] ~= nil then
                local d = d2(c.x[k], c.y[k], cx, cy)
                if d < best then
                    px, py, best = c.x[k], c.y[k], d
                end
            end
        end
    end
    -- two tiles from the tower towards the town (the units stand under it, not in its door)
    local dx, dy = view.town_x - px, view.town_y - py
    local l = math.max(math.abs(dx), math.abs(dy))
    if l > 0 then
        px, py = px + dx * 2 // l, py + dy * 2 // l
    end
    if g.post_x ~= px or g.post_y ~= py then
        g.post_x, g.post_y = px, py
        g.x, g.y = px, py
    end
end

-- the raids in the zone, with the towers' fire on each
local function raids_in_zone(view, zone)
    local out = {}
    for _, cl in ipairs(D.clusters()) do
        local inside = d2(cl.x, cl.y, view.town_x, view.town_y) <= zone * zone
        if not inside then
            inside = #ai.units{category = "building", near = {cl.x, cl.y, M.opts.building_r}, limit = 1} > 0
        end
        if inside then
            local thp, tatk = 0, 0
            local covered = false
            for _, id in ipairs(ai.units{category = "building", armed = true, built = true, near = {cl.x, cl.y, 8}, limit = 6}) do
                local t = ai.unit(id)
                if t ~= nil then
                    local r = (t.range or 0) // 100 + 1
                    if d2(t.x, t.y, cl.x, cl.y) <= r * r then
                        covered = true
                        thp, tatk = thp + t.hp // 4, tatk + (t.attack or 0) // 2
                    end
                end
            end
            out[#out + 1] = {cl = cl, thp = thp, tatk = tatk, covered = covered}
        end
    end
    return out
end

-- [owner, AI_RESEARCH §8.27 naval_flee, lib.groups' avoid_ships] under a warship's fire only ranged units answer: a
-- ranged unit shoots a ship within its range + 2; the others within the ship's reach + 1 walk out of it (to the town
-- centre when that lies out of its reach, else 4 tiles past it, straight away from the ship). -> the units left for
-- the land fight
local function ships(view, g, recs)
    local ids = ai.seen{enemy = true, category = "ship", near = {g.post_x, g.post_y, 20}, limit = 12}
    if #ids == 0 then
        return recs
    end
    local c = ai.get(ids, {"x", "y", "range", "attack"})
    local list = {}
    for k = 1, #ids do
        if c.x[k] ~= nil and (c.attack[k] or 0) > 0 then
            list[#list + 1] = {id = ids[k], x = c.x[k], y = c.y[k], r = (c.range[k] or 0) // 100}
        end
    end
    if #list == 0 then
        return recs
    end
    local left, shoot, moves = {}, {}, {}
    for _, u in ipairs(recs) do
        local s, sd = nil, nil
        for _, sh in ipairs(list) do
            local d = d2(u.x, u.y, sh.x, sh.y)
            if sd == nil or d < sd then
                s, sd = sh, d
            end
        end
        local reach = s.r + 1
        local ur = u.range // 100 + 2
        if u.range >= 150 and sd <= ur * ur then
            shoot[s.id] = shoot[s.id] or {}
            local l = shoot[s.id]
            l[#l + 1] = u.id
            g.aims[u.id] = s.id
        elseif sd <= reach * reach then
            local tx, ty = view.town_x, view.town_y
            if d2(tx, ty, s.x, s.y) <= (reach + 2) * (reach + 2) then
                local dx, dy = u.x - s.x, u.y - s.y
                local l = math.max(math.abs(dx), math.abs(dy), 1)
                tx, ty = s.x + dx * (reach + 4) // l, s.y + dy * (reach + 4) // l
            end
            local key = tx * 10000 + ty
            moves[key] = moves[key] or {}
            local l = moves[key]
            l[#l + 1] = u.id
            g.aims[u.id] = nil
        else
            left[#left + 1] = u
        end
    end
    for _, sid in ipairs(ai.keys(shoot)) do
        ai.attack(shoot[sid], sid)
    end
    for _, key in ipairs(ai.keys(moves)) do
        ai.move(moves[key], key // 10000, key % 10000, {forced = true})
    end
    return left
end

-- the guard hunts the enemy citizens in its zone that no known enemy tower covers: 2 units a citizen, 4 citizens and
-- half the guard at most, the nearest first -> whether it hunts
local function deny(view, g, recs, zone)
    local ids = ai.seen{enemy = true, category = "citizen", near = {view.town_x, view.town_y, zone}, limit = 12}
    if #ids == 0 then
        g.hunt = nil
        return false
    end
    local c = ai.get(ids, {"x", "y"})
    local prey = {}
    for k = 1, #ids do
        if c.x[k] ~= nil and #prey < M.opts.deny_n then
            local safe = true
            for _, m in ipairs(ai.memory{buildings = true, enemy = true, near = {c.x[k], c.y[k], 9}, limit = 6}) do
                local cl = ai.class(m.type)
                local r = (cl ~= nil and cl.range or 0) // 100 + 2
                if cl ~= nil and (cl.attack or 0) > 0 and d2(m.x, m.y, c.x[k], c.y[k]) <= r * r then
                    safe = false
                end
            end
            if safe then
                prey[#prey + 1] = {id = ids[k], x = c.x[k], y = c.y[k]}
            end
        end
    end
    if #prey == 0 then
        g.hunt = nil
        return false
    end
    local healthy = {}
    for _, u in ipairs(recs) do
        if not g.hurt[u.id] then
            healthy[#healthy + 1] = u
        end
    end
    local most = math.min(#healthy // 2, #prey * 2)
    if most == 0 then
        return false
    end
    local by, used = {}, {}
    local n = 0
    for _, p in ipairs(prey) do
        for _ = 1, 2 do
            if n < most then
                local best, bd = nil, nil
                for _, u in ipairs(healthy) do
                    if not used[u.id] then
                        local d = d2(u.x, u.y, p.x, p.y)
                        if bd == nil or d < bd then
                            best, bd = u, d
                        end
                    end
                end
                if best ~= nil then
                    used[best.id] = true
                    n = n + 1
                    if g.aims[best.id] ~= p.id or (best.action ~= "attack" and best.action ~= "move") then
                        by[p.id] = by[p.id] or {}
                        local l = by[p.id]
                        l[#l + 1] = best.id
                    end
                    g.aims[best.id] = p.id
                end
            end
        end
    end
    for _, tid in ipairs(ai.keys(by)) do
        ai.attack(by[tid], tid)
    end
    if g.hunt == nil then
        print(string.format("guard deny prey=%d units=%d", #prey, n))
    end
    g.hunt = view.time
    g.state = "hunting"
    return true
end

function M.step(view, mem, opts)
    opts = opts or {}
    if view.town_x < 0 then
        return
    end
    local g = G.role(mem, "guard")
    local free = G.free_soldiers(view.town_x, view.town_y)
    if g == nil and #free == 0 then
        return
    end
    if g == nil then
        g = G.create(mem, {task = "defend", role = "guard", x = view.town_x, y = view.town_y, manual = true})
        g.hurt = {}
    end
    if #free > 0 then
        G.add(mem, g, free)
        g.state = "arrived"   -- (lib.groups forms a new group; the guard walks to its post instead)
    end
    g.hurt = g.hurt or {}
    if #g.units == 0 then
        return
    end
    choose_post(view, mem, g)
    local zone = opts.zone or view.claim_range
    local now = view.time
    -- the members
    local ids = {}
    for _, id in ipairs(g.units) do
        if not G.committed[id] then
            ids[#ids + 1] = id
        end
    end
    local c = ai.get(ids, {"x", "y", "hp", "hp_max", "attack", "range", "action", "inside", "type"})
    local recs = {}
    local ghp, gatk = 0, 0
    for k = 1, #ids do
        if c.x[k] ~= nil and c.inside[k] == nil then
            local u = {id = ids[k], x = c.x[k], y = c.y[k], hp = c.hp[k] or 0, hp_max = c.hp_max[k] or 1,
                       attack = c.attack[k] or 0, range = c.range[k] or 0, action = c.action[k], type = c.type[k]}
            recs[#recs + 1] = u
            if g.hurt[u.id] == nil and u.hp * 100 < u.hp_max * M.opts.hurt_pct then
                g.hurt[u.id] = true
            end
            if not g.hurt[u.id] then
                ghp, gatk = ghp + u.hp, gatk + u.attack
            end
        end
    end
    recs = ships(view, g, recs)
    local raids = raids_in_zone(view, zone)
    -- the raids to fight: at the odds with the towers, or under a tower
    local fight = {}
    for _, r in ipairs(raids) do
        local enemy = r.cl.hp * r.cl.atk
        local own = (ghp + r.thp) * (gatk + r.tatk)
        if r.covered or own * 100 >= enemy * M.opts.engage_odds then
            fight[#fight + 1] = r
        end
        -- the raids that came teach the mix (ai.get on the raid's units: their classes)
        if g.noted == nil or now - g.noted >= 20000 then
            local rc = ai.get(r.cl.ids, {"type", "range"})
            mem.guard_raids = mem.guard_raids or {ranged = 0, pierce = 0, shock = 0, cavalry = 0}
            for k = 1, #r.cl.ids do
                if rc.type[k] ~= nil then
                    local role = role_of(rc.type[k], rc.range[k])
                    mem.guard_raids[role] = math.min(mem.guard_raids[role] + 1, 40)
                end
            end
        end
    end
    if #raids > 0 and (g.noted == nil or now - g.noted >= 20000) then
        g.noted = now
    end
    if #fight == 0 and #raids == 0 and M.opts.deny then
        -- [research §4.3 step 4; boom's old defend group] no raid: the enemy citizens gathering or building in the
        -- zone, out of their towers' reach, are hunted by part of the guard (the overlap of two claims on a Small
        -- map: the old group's chase of them kept a neighbour's economy down)
        if deny(view, g, recs, zone) then
            return
        end
    end
    if #fight == 0 then
        -- nothing to fight: everyone home, the hurt forgiven (they stand under the tower anyway)
        if #raids == 0 then
            g.hurt = {}
        end
        g.aims = {}
        if g.state ~= "arrived" or now - (g.home_t or 0) >= 10000 then
            g.home_t = now
            local strays = {}
            for _, u in ipairs(recs) do
                if d2(u.x, u.y, g.post_x, g.post_y) > M.opts.home_r * M.opts.home_r
                        and (u.action == "idle" or u.action == "attack" or g.state ~= "arrived") then
                    strays[#strays + 1] = u.id
                end
            end
            if #strays > 0 then
                ai.move(strays, g.post_x, g.post_y, {forced = #raids > 0})
            end
            g.state = "arrived"
        end
        return
    end
    g.state = "fighting"
    -- the targets: every raider of the raids fought, the one nearest the own citizens first (then the weakest)
    local targets = {}
    for _, r in ipairs(fight) do
        local cl = r.cl
        local cits = ai.units{category = "citizen", near = {cl.x, cl.y, 14}, limit = 30}
        local cc = #cits > 0 and ai.get(cits, {"x", "y"}) or {x = {}, y = {}}
        local rc = ai.get(cl.ids, {"hp", "x", "y"})
        for k = 1, #cl.ids do
            if rc.x[k] ~= nil then
                local dc = 400
                for j = 1, #cits do
                    if cc.x[j] ~= nil then
                        local d = d2(rc.x[k], rc.y[k], cc.x[j], cc.y[j])
                        if d < dc then
                            dc = d
                        end
                    end
                end
                targets[#targets + 1] = {id = cl.ids[k], x = rc.x[k], y = rc.y[k], hp = rc.hp[k] or 1, key = dc // 9}
            end
        end
    end
    table.sort(targets, function(a, b)
        if a.key ~= b.key then
            return a.key < b.key
        end
        if a.hp ~= b.hp then
            return a.hp < b.hp
        end
        return a.id < b.id
    end)
    local valid = {}
    for _, t in ipairs(targets) do
        valid[t.id] = t
    end
    -- the hurt pull back; the others: a unit on a valid target keeps it, the rest go on the targets in turn
    local back = {}
    local fire = {}
    local free_units = {}
    for _, u in ipairs(recs) do
        if g.hurt[u.id] then
            if d2(u.x, u.y, g.post_x, g.post_y) > 9 then
                back[#back + 1] = u.id
            end
            g.aims[u.id] = nil
        else
            local aim = g.aims[u.id]
            if aim ~= nil and valid[aim] ~= nil and (u.action == "attack" or u.action == "move") then
                fire[aim] = (fire[aim] or 0) + u.attack
            else
                free_units[#free_units + 1] = u
            end
        end
    end
    if #back > 0 then
        ai.move(back, g.post_x, g.post_y, {forced = true})
    end
    local by_target = {}
    local ti = 1
    for _, u in ipairs(free_units) do
        -- the next target still short of fire (the last one takes the rest)
        while ti < #targets and (fire[targets[ti].id] or 0) * M.opts.focus_hits >= targets[ti].hp do
            ti = ti + 1
        end
        local t = targets[ti]
        if t ~= nil then
            fire[t.id] = (fire[t.id] or 0) + u.attack
            if g.aims[u.id] ~= t.id or (u.action ~= "attack" and u.action ~= "move") then
                by_target[t.id] = by_target[t.id] or {}
                local l = by_target[t.id]
                l[#l + 1] = u.id
            end
            g.aims[u.id] = t.id
        end
    end
    for _, tid in ipairs(ai.keys(by_target)) do
        ai.attack(by_target[tid], tid)
    end
    if now - (g.fight_log or 0) >= 15000 then
        g.fight_log = now
        print(string.format("guard fight units=%d hurt=%d raids=%d targets=%d post=%d,%d", #recs, #back, #fight, #targets,
            g.post_x, g.post_y))
    end
end

return M

-- The town's answer to raids on its citizens, over the per-unit interface (docs/AI_LUA_UNITS.md §3, §11;
-- docs/AI_RESEARCH.md §8.41). require("lib.defence"). Generic: any script may use it; the numbers are options.
--
--   D.setup(opts)           once (the script's init): the options; the engine's militia and danger-zone reflexes off
--                           (ai.reflex), since this module does both
--   D.micro(mem, events, now, town_x, town_y)
--                           every micro step (250 game ms) with that step's ai.events(): the hits, the raider scan
--                           once a game second, the citizens in danger flee or fight, those away go back
--   D.step(view, mem)       every strategy step: the tower repairs, the trace lines
--   D.clusters()            the raider clusters of the last scan (the guard reads them): {x, y, n, hp, atk, ids}
--   D.on_return(list)       THE RETURN HOOK: the citizens whose danger is over, {{id, x, y, act}...} (x, y: the spot
--                           they worked at; act: "gather", "build" or "other"). The default sends each gatherer back
--                           to the resource at its spot (ai.gather) and hands it back to the engine's gatherer lists
--                           (ai.release{gather = true}); the rest go back to the engine idle. An economy module that
--                           places its gatherers itself replaces it: `D.on_return = function(list) ... end`.
--
-- The rules and where they come from:
-- - [owner, research §4.3 step 2] only the citizens in danger move: a citizen with a raider within `danger_r` of
--   it or of the spot it works at. The other sites keep working.
-- - [research §4.3 step 3, ageofai tactics.lua tower_bait] they run to the nearest own tower (to the spot behind it
--   from the raiders: the raiders that follow come under its fire) or town centre that is not further into the raid;
--   no garrison (in this game only a fortress shelters citizens; a settlement's garrison upgrades it).
-- - [balanced: military.militia, militia_towers; research §4.3 step 3] the citizens fight only when the odds say
--   they win: their strength with the soldiers near and the towers' fire, as Lanchester's square law (summed hit
--   points x summed attack, the towers' attack by their reload), at `militia_odds` (3 x: the research's; balanced's
--   1.5 x sent the citizens to die, AI_RESEARCH §8.33 and §8.38), at most 4 citizens a raider (3 at 3 x the
--   research's, but three citizens against a clubman are 2.7 x by the square law) and 6 raiders. The soldiers and
--   towers alone at `hold_odds` leave the citizens out of it.
-- - [owner] they go back to their exact spot when it is clear (`clear_r`, no hit near for `clear_ms`).
-- - [owner] builders repair the towers hit, during and after a raid (at most 2 a tower, 3 towers at once).
-- - [boom THREAT_MIN, the engine's kMaxRaiders] a group under `raid_min` (a scout) is a raid only once it has hit an
--   own unit.
-- - [ageofai safety.lua, PORT_LEARNINGS "the micro step's instruction budget"] per-raid queries, not per citizen:
--   one ai.seen a second and a few ai.units round each raid, so the cost does not grow with the town.
local M = {}

M.opts = {
    scan_r = 45,          -- tiles round the town the raiders are looked for (the claim range and a margin)
    raid_min = 2500,      -- a group weaker than this (hit points x attack; a scout) is a raid only once it hits (boom's
                          -- THREAT_MIN: a scout walking through the town sent 60 citizens running in 5 minutes)
    hostile_ms = 15000,   -- a unit that hit an own unit this recently makes its group a raid
    cluster_r = 8,        -- raiders this near each other are one raid
    danger_r = 6,         -- a citizen (or its work spot) this near a raider is in danger
    near_r = 10,          -- the citizens and soldiers this near a raid count in its odds
    hold_odds = 130,      -- percent: the soldiers and towers alone hold the raid (the citizens only run)
    militia_odds = 300,   -- percent: the citizens join when the odds with them reach this ...
    militia_per = 4,      -- ... at most this many a raider ...
    militia_max = 6,      -- ... against at most this many raiders
    militia_ms = 12000,   -- a militia citizen is back at work after this long, or once its raid is gone
    militia_ranged = false, -- the militia against a raid with ranged units (rock throwers and slingers kite the
                          -- citizens: seed 4 against the classic lost a citizen in each such fight, odds or not) ...
    wave_n = 8,           -- a raid of this many or more (0: off) is a wave: the citizens run away from it, to a
                          -- refuge further from it (the settlements too), not to the nearest tower in its path
    bait = false,         -- [ageofai tactics.lua tower_bait, upstream early.c] a citizen with an enemy soldier within
                          -- 5 tiles, out of its towers' reach, runs to the spot 1.5 tiles behind the nearest
                          -- shooting building within 25 (to the Copper Age): the raider follows under its fire
    hurt_safety = false,  -- [ageofai safety.lua, upstream attack.c citizens_safety] a work spot within 6 tiles of a
                          -- citizen hit in the last 2 minutes (20 s with the attacker in sight; minerals: 3 hits) is
                          -- dangerous unless the town holds it (raid_held): its gatherers go to the nearest drop
                          -- site, and M.dangerous tells the script's claims to leave the site while it lasts
    raid_held = 150,      -- percent: own soldiers and towers within 8 against the raiders within 8
    tower_militia = false, -- (off; AI_RESEARCH §8.41 D7) in the Prehistoric, a raid in a tower's reach is fought by
                          -- the militia whatever its size or kind, when the citizens and the towers win by the
                          -- square law at militia_odds (the caps of 6 raiders and of ranged raids lifted there)
    wave_size = 8,        -- a raid this large is a wave for the scripts (boom's plan.emergency)
    camp_ms = 40000,      -- ... unless the raid has camped this long within camp_r of one spot: 1-3 rock throwers
    camp_r = 10,          -- by a food patch for 15 minutes starved two towns that fled them and never fought
                          -- (seeds 18 and 25 against the improved classic, AI_RESEARCH §8.41)
    tower_share = 100,    -- percent of a tower's attack per 2 s of its reload counted in the odds
    clear_r = 10,         -- a spot is clear with no raider this near ...
    clear_ms = 8000,      -- ... and no citizen hit near it for this long
    away_min = 6000,      -- a citizen stays away at least this long
    away_max = 150000,    -- and at most this long (then the engine has it again)
    reflee_ms = 3000,     -- a citizen still in danger is sent again after this long
    return_scan = 40,     -- citizens away looked at a second at most, in turn (boom-econ: 100+ citizens away with a
                          -- large raid overran the micro step's budget from minute 81 of seed 19)
    return_max = 12,      -- citizens sent back at most a second (the micro step's 300k budget: 60 at once
                          -- overran it in one 120-minute game)
    repair = true,        -- builders repair the towers hit
    repair_pct = 85,      -- a tower under this share of its hit points is repaired ...
    repair_n = 2,         -- ... by this many citizens ...
    repair_towers = 3,    -- ... at this many towers at once
    repair_ms = 90000,    -- a repairer goes back after this long at most
}

local CLUSTERS = {}       -- the last scan's raids (rebuilt every game second; not saved: a load rescans)
local REFUGES = {}        -- own built towers and town centres of the last scan: {id, x, y, tower}

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

-- the class data (cached per game: static)
local CLASS = {}
local function class(t)
    local c = CLASS[t]
    if c == nil then
        c = ai.class(t) or {}
        CLASS[t] = c
    end
    return c
end

function M.setup(opts)
    for k, v in pairs(opts or {}) do
        M.opts[k] = v
    end
    if ai.reflex ~= nil then
        ai.reflex{militia = false, danger = false}   -- this module is the militia and the flight
    end
end

-- (the land raids only: the guard does not chase warships)
function M.clusters()
    local out = {}
    for _, cl in ipairs(CLUSTERS) do
        if not cl.ship then
            out[#out + 1] = cl
        end
    end
    return out
end

local function state(mem)
    mem.away = mem.away or {}
    mem.hurt = mem.hurt or {}
    mem.hurt_at = mem.hurt_at or 0
    mem.attackers = mem.attackers or {}
    mem.st = mem.st or {fled = 0, back = 0, militia = 0, repair = 0, raids = 0, lost_cit = 0, lost_sol = 0,
                        sol_by_tower = 0, hits = 0}
    return mem
end

-- ---------------------------------------------------------------------------------------------------------------
-- The scan: the raiders, clustered; the refuges

local function scan(tx, ty)
    CLUSTERS, REFUGES = {}, {}
    local o = M.opts
    -- (warships too, in their own clusters: the citizens a fled raid sent back to a shore under a warship's fire
    -- died there - 68 to 274 citizens killed by ships against the classic before they counted)
    local ids = ai.seen{enemy = true, armed = true, category = "soldier", near = {tx, ty, o.scan_r}}
    local nsol = #ids
    for _, id in ipairs(ai.seen{enemy = true, armed = true, category = "ship", near = {tx, ty, o.scan_r}, limit = 16}) do
        ids[#ids + 1] = id
    end
    if #ids > 0 then
        local c = ai.get(ids, {"x", "y", "hp", "attack", "range"})
        for k = 1, #ids do
            local x, y = c.x[k], c.y[k]
            local ship = k > nsol
            if x ~= nil then
                local cl = nil
                for _, q in ipairs(CLUSTERS) do
                    if cl == nil and q.ship == ship and d2(x, y, q.sx, q.sy) <= o.cluster_r * o.cluster_r then
                        cl = q
                    end
                end
                if cl == nil then
                    cl = {sx = x, sy = y, ids = {}, xs = {}, ys = {}, hp = 0, atk = 0, n = 0, ranged = 0, tx = 0, ty = 0,
                          ship = ship, dr = o.danger_r}
                    CLUSTERS[#CLUSTERS + 1] = cl
                end
                if ship then
                    cl.dr = math.max(cl.dr, (c.range[k] or 0) // 100 + 2)
                end
                cl.n = cl.n + 1
                cl.ids[cl.n], cl.xs[cl.n], cl.ys[cl.n] = ids[k], x, y
                cl.hp = cl.hp + (c.hp[k] or 0)
                cl.atk = cl.atk + (c.attack[k] or 0)
                cl.tx, cl.ty = cl.tx + x, cl.ty + y
                if (c.range[k] or 0) >= 150 then
                    cl.ranged = cl.ranged + 1
                end
            end
        end
        for _, cl in ipairs(CLUSTERS) do
            cl.x, cl.y = cl.tx // cl.n, cl.ty // cl.n
            cl.x0, cl.x1, cl.y0, cl.y1 = cl.xs[1], cl.xs[1], cl.ys[1], cl.ys[1]
            for k = 2, cl.n do
                cl.x0, cl.x1 = math.min(cl.x0, cl.xs[k]), math.max(cl.x1, cl.xs[k])
                cl.y0, cl.y1 = math.min(cl.y0, cl.ys[k]), math.max(cl.y1, cl.ys[k])
            end
        end
    end
    -- the refuges: built own towers (armed buildings) and town centres near the town
    local tw = ai.units{category = "building", armed = true, built = true, near = {tx, ty, o.scan_r + 10}, limit = 32}
    if #tw > 0 then
        local c = ai.get(tw, {"x", "y", "range", "type"})
        for k = 1, #tw do
            if c.x[k] ~= nil then
                REFUGES[#REFUGES + 1] = {id = tw[k], x = c.x[k], y = c.y[k], tower = true, range = (c.range[k] or 0) // 100}
            end
        end
    end
    for _, t in ipairs({"b_town_center", "b_capitol", "b_settlement"}) do
        for _, id in ipairs(ai.units{type = t, built = true, limit = 8}) do
            local u = ai.unit(id)
            if u ~= nil then
                REFUGES[#REFUGES + 1] = {id = id, x = u.x, y = u.y, tower = false, range = 0, drop = t == "b_settlement"}
            end
        end
    end
end

-- the nearest raider of a cluster to a tile -> its squared distance
local function raider_d2(cl, x, y)
    local best = nil
    for k = 1, cl.n do
        local d = d2(x, y, cl.xs[k], cl.ys[k])
        if best == nil or d < best then
            best = d
        end
    end
    return best or 1000000
end

-- any raider of any cluster within r of a tile
local function raided(x, y, r)
    for _, cl in ipairs(CLUSTERS) do
        local rr = r + cl.dr - M.opts.danger_r
        if cl.x0 ~= nil and (x < cl.x0 - rr or x > cl.x1 + rr or y < cl.y0 - rr or y > cl.y1 + rr) then
            -- (outside the raid's box grown by the radius: no raider can be that near)
        elseif raider_d2(cl, x, y) <= rr * rr then
            return true
        end
    end
    return false
end

-- citizens hit near a tile in the last ms
local function hurt_near(mem, x, y, r, now, ms)
    for _, h in ipairs(mem.hurt) do
        if now - h.t <= ms and d2(x, y, h.x, h.y) <= r * r then
            return true
        end
    end
    return false
end

-- where a citizen at (x, y) runs from a raid centred at (rx, ry): the nearest refuge not further into the raid (a
-- tower may be: the raiders that follow come under its fire), to the spot 2 tiles behind it from the raid
local function refuge_for(x, y, rx, ry, ship, wave)
    local best, bd = nil, nil
    local dc = d2(x, y, rx, ry)
    for _, r in ipairs(REFUGES) do
        local dr = d2(r.x, r.y, rx, ry)
        local ok
        if wave then
            ok = dr >= dc + 16   -- (a wave: only away from it, settlements too)
        else
            ok = not r.drop and ((r.tower and not ship) or dr >= dc)
        end
        if ok then   -- (from a warship: away from it, a tower outranged)
            local d = d2(x, y, r.x, r.y)
            if bd == nil or d < bd then
                best, bd = r, d
            end
        end
    end
    if best == nil then
        if wave then   -- (no refuge away from the wave: 14 tiles straight away from it, on the map)
            local m = ai.map()
            local dx, dy = x - rx, y - ry
            local l = math.max(math.abs(dx), math.abs(dy), 1)
            return math.max(2, math.min(x + dx * 14 // l, m.width - 3)), math.max(2, math.min(y + dy * 14 // l, m.height - 3))
        end
        return nil
    end
    local dx, dy = best.x - rx, best.y - ry
    local l = math.max(math.abs(dx), math.abs(dy), 1)
    return best.x + dx * 2 // l, best.y + dy * 2 // l
end

-- ---------------------------------------------------------------------------------------------------------------
-- Away and back

-- the spot a citizen works at (its goal's place while it gathers or builds) and what it does
local function work_spot(u)
    if (u.action == "gather" or u.action == "build" or u.action == "repair") and u.tx ~= nil then
        return u.tx, u.ty, u.action == "gather" and "gather" or "build"
    end
    return u.x, u.y, "other"
end

-- the default return: gatherers back to the resource at their spot, handed back to the engine's lists
function M.on_return(list)
    local by_res, rest = {}, {}
    for _, b in ipairs(list) do
        local res = nil
        if b.act == "gather" then
            res = ai.resources{near = {b.x, b.y, 2}, limit = 1}[1]
        end
        if res ~= nil then
            by_res[res] = by_res[res] or {}
            local l = by_res[res]
            l[#l + 1] = b.id
        else
            rest[#rest + 1] = b.id
        end
    end
    for _, res in ipairs(ai.keys(by_res)) do
        local ids = by_res[res]
        if ai.gather(ids, res) ~= "ok" then
            for _, id in ipairs(ids) do
                rest[#rest + 1] = id
            end
        else
            ai.release(ids, {gather = true})
        end
    end
    if #rest > 0 then
        ai.stop(rest)
        ai.release(rest)
    end
end

local function go_away(mem, u, why, now)
    local a = mem.away[u.id]
    if a == nil then
        local x, y, act = work_spot(u)
        a = {x = x, y = y, act = act, t = now, why = why, sent = 0}
        mem.away[u.id] = a
    else
        a.why = why
    end
    return a
end

-- the citizens away whose danger is over go back (through M.on_return); the dead are forgotten
local function returns(mem, now)
    local o = M.opts
    local back = {}
    local keys = ai.keys(mem.away)
    local start = #keys > 0 and (mem.return_k or 0) % #keys or 0
    local look = {}
    for i = 1, math.min(#keys, o.return_scan) do
        look[i] = keys[(start + i - 1) % #keys + 1]
    end
    mem.return_k = start + #look
    for _, id in ipairs(look) do
        local a = mem.away[id]
        local u = ai.unit(id)
        if u == nil then
            mem.away[id] = nil
        else
            local done = false
            if a.why == "militia" then
                done = now >= a.t + o.militia_ms or not raided(u.x, u.y, o.near_r)
            elseif a.why == "bait" then
                done = now >= a.t + 20000 or (math.abs(u.x - a.bx) <= 2 and math.abs(u.y - a.by) <= 2)
            elseif a.why == "hurt" then
                done = now >= a.t + o.away_max or (now >= a.t + o.away_min and not M.dangerous(mem, a.x, a.y, now))
            elseif a.why == "repair" then
                local t = ai.unit(a.tower or -1)
                done = t == nil or t.hp * 100 >= t.hp_max * 98 or now >= a.t + o.repair_ms
                    or (u.action ~= "repair" and u.action ~= "move" and now >= a.t + 5000)
            else
                done = now >= a.t + o.away_max
                    or (now >= a.t + o.away_min and not raided(a.x, a.y, o.clear_r) and not raided(u.x, u.y, o.danger_r)
                        and not hurt_near(mem, a.x, a.y, o.danger_r, now, o.clear_ms))
            end
            if done and #back < o.return_max then
                back[#back + 1] = {id = id, x = a.x, y = a.y, act = a.act}
                mem.away[id] = nil
            end
        end
    end
    if #back > 0 then
        mem.st.back = mem.st.back + #back
        M.on_return(back)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- One raid: fight or run

local function own_side(mem, cl, now)
    local o = M.opts
    -- the citizens near: the engine's (working) and those this module has away already
    local cits = ai.units{category = "citizen", near = {cl.x, cl.y, o.near_r + o.danger_r}, limit = 40}
    local recs = {}
    for _, id in ipairs(cits) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and (u.held == "engine" or mem.away[id] ~= nil) then
            recs[#recs + 1] = u
        end
    end
    -- the soldiers near (the guard's and the engine's), the towers whose reach the raid stands in
    local shp, satk = 0, 0
    local sol = ai.units{category = "soldier", near = {cl.x, cl.y, o.near_r + 2}, limit = 40}
    if #sol > 0 then
        local c = ai.get(sol, {"hp", "attack"})
        for k = 1, #sol do
            shp, satk = shp + (c.hp[k] or 0), satk + (c.attack[k] or 0)
        end
    end
    local thp, tatk = 0, 0
    for _, r in ipairs(REFUGES) do
        if r.tower and d2(r.x, r.y, cl.x, cl.y) <= (r.range + 1) * (r.range + 1) then
            local u = ai.unit(r.id)
            if u ~= nil then
                local reload = class(u.type).reload or 2
                tatk = tatk + (u.attack or 0) * 2 * o.tower_share // (100 * math.max(reload, 1))
                thp = thp + u.hp // 4   -- (the raiders shoot the citizens first: a tower's hit points count a quarter)
            end
        end
    end
    return recs, shp + thp, satk + tatk
end

local function handle(mem, cl, now)
    local o = M.opts
    -- [research §1.6] ranged raiders keep out of the citizens' reach and wear armour against their throw: their
    -- strength counts up to twice (all ranged: twice)
    local enemy = cl.hp * cl.atk * (100 + 100 * cl.ranged // cl.n) // 100
    if enemy <= 0 then
        return
    end
    local recs, hp, atk = own_side(mem, cl, now)
    if #recs == 0 then
        return
    end
    local held = hp * atk * 100 >= enemy * o.hold_odds
    -- the militia fighting this raid already, and the citizens in danger (nearest the raid first)
    local danger, militia = {}, {}
    local fhp, fatk = hp, atk
    for _, u in ipairs(recs) do
        local a = mem.away[u.id]
        if a ~= nil and a.why == "militia" then
            -- (a militia citizen belongs to the raid nearest it: another raid near does not call it off)
            local mine = true
            local du = raider_d2(cl, u.x, u.y)
            for _, q in ipairs(CLUSTERS) do
                if q ~= cl and raider_d2(q, u.x, u.y) < du then
                    mine = false
                end
            end
            if mine then
                militia[#militia + 1] = u
                fhp, fatk = fhp + u.hp, fatk + (u.attack or 0)
            end
        else
            local sx, sy = work_spot(u)
            if a ~= nil then
                sx, sy = a.x, a.y
            end
            local du = raider_d2(cl, u.x, u.y)
            if du <= cl.dr * cl.dr or (a == nil and raider_d2(cl, sx, sy) <= cl.dr * cl.dr) then
                danger[#danger + 1] = {u = u, d = du}
            end
        end
    end
    -- the militia losing (under half the odds it joined at) breaks off and runs with the rest
    if #militia > 0 and (held or fhp * fatk * 200 < enemy * o.militia_odds) then
        for _, u in ipairs(militia) do
            local a = mem.away[u.id]
            a.why = "flee"
            a.sent = 0
            danger[#danger + 1] = {u = u, d = raider_d2(cl, u.x, u.y)}
        end
        if not held then
            print(string.format("def militia_off n=%d raiders=%d at=%d,%d", #militia, cl.n, cl.x, cl.y))
        end
        militia = {}
        fhp, fatk = hp, atk
    end
    if #danger == 0 then
        return
    end
    table.sort(danger, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.u.id < b.u.id
    end)
    -- the militia: the nearest citizens join while the odds are short of militia_odds, within its caps
    local fighters = {}
    local in_reach = false
    if o.tower_militia and (mem.epoch or 0) == 0 then
        for _, r in ipairs(REFUGES) do
            if r.tower and d2(r.x, r.y, cl.x, cl.y) <= (r.range + 1) * (r.range + 1) then
                in_reach = true
            end
        end
    end
    if not held and (in_reach or (cl.n <= o.militia_max and (o.militia_ranged or cl.ranged == 0 or (cl.camped or 0) >= o.camp_ms)))
            and fhp * fatk * 100 < enemy * o.militia_odds then
        local nhp, natk = fhp, fatk
        for _, c in ipairs(danger) do
            if #militia + #fighters < cl.n * o.militia_per and nhp * natk * 100 < enemy * o.militia_odds then
                fighters[#fighters + 1] = c.u
                nhp, natk = nhp + c.u.hp, natk + (c.u.attack or 0)
            end
        end
        if nhp * natk * 100 < enemy * o.militia_odds then
            fighters = {}
        end
    end
    if #fighters > 0 then
        -- focus: every militia citizen on the raider nearest the citizens' first one
        local f0 = fighters[1]
        local tgt, td = nil, nil
        for k = 1, cl.n do
            local d = d2(f0.x, f0.y, cl.xs[k], cl.ys[k])
            if td == nil or d < td then
                tgt, td = cl.ids[k], d
            end
        end
        local ids = {}
        for _, u in ipairs(fighters) do
            ids[#ids + 1] = u.id
            go_away(mem, u, "militia", now).t = now
        end
        if ai.attack(ids, tgt) == "ok" then
            mem.st.militia = mem.st.militia + #ids
            if now - (mem.militia_log or 0) >= 10000 then
                mem.militia_log = now
                print(string.format("def militia n=%d raiders=%d ranged=%d odds=%d at=%d,%d", #ids + #militia, cl.n,
                    cl.ranged, hp * atk * 100 // enemy, cl.x, cl.y))
            end
        end
    end
    -- the rest in danger run
    local in_fight = {}
    for _, u in ipairs(fighters) do
        in_fight[u.id] = true
    end
    local moves = {}
    local fled = 0
    for _, c in ipairs(danger) do
        local u = c.u
        if not in_fight[u.id] then
            local a = mem.away[u.id]
            if a == nil or now - (a.sent or 0) >= o.reflee_ms then
                local fx, fy = refuge_for(u.x, u.y, cl.x, cl.y, cl.ship, o.wave_n > 0 and cl.n >= o.wave_n)
                if fx ~= nil and d2(u.x, u.y, fx, fy) > 4 then
                    a = go_away(mem, u, "flee", now)
                    a.t, a.sent = now, now
                    local key = fx * 10000 + fy
                    moves[key] = moves[key] or {}
                    local l = moves[key]
                    l[#l + 1] = u.id
                    fled = fled + 1
                elseif a ~= nil then
                    a.t = now   -- at the refuge already: it waits there
                end
            end
        end
    end
    for _, key in ipairs(ai.keys(moves)) do
        ai.move(moves[key], key // 10000, key % 10000, {forced = true})
    end
    if fled > 0 then
        mem.st.fled = mem.st.fled + fled
        if now - (mem.flee_log or 0) >= 10000 then
            mem.flee_log = now
            print(string.format("def flee n=%d raiders=%d ranged=%d odds=%d held=%s at=%d,%d", fled, cl.n, cl.ranged,
                hp * atk * 100 // enemy, tostring(held), cl.x, cl.y))
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The micro step

-- the hits of the step's events: citizens hit by an enemy (not an animal) mark their spot; deaths are counted
local function note(mem, events, now)
    local looked = 0
    for _, e in ipairs(events) do
        if e.kind == "attacked" and looked < 24 and (e.player or 0) > 0 then
            local v = ai.unit(e.unit)
            looked = looked + 1
            if e.attacker ~= nil then
                mem.attackers[e.attacker] = now
            end
            if v ~= nil and v.category == "citizen" then
                local k = mem.hurt_at % 32 + 1
                local seen = false
                if M.opts.hurt_safety then   -- [ageofai safety.lua hurt_seen_short]
                    seen = #ai.seen{enemy = true, armed = true, category = "soldier", near = {v.x, v.y, 6}, limit = 1} > 0
                end
                mem.hurt[k] = {x = v.x, y = v.y, t = now, seen = seen}
                mem.hurt_at = mem.hurt_at + 1
                mem.st.hits = mem.st.hits + 1
            end
        elseif (e.kind == "died" or e.kind == "converted") and e.type ~= nil then
            -- (a converted unit is lost as well; before the fix of 2026-11-10 the engine reported every kill as
            -- "converted", AI_RESEARCH §8.41)
            local c = class(e.type)
            if c.category == "citizen" then
                mem.st.lost_cit = mem.st.lost_cit + 1
            elseif c.category == "soldier" then
                mem.st.lost_sol = mem.st.lost_sol + 1
                local k = e.killer ~= nil and ai.unit(e.killer) or nil
                if k ~= nil and k.category == "building" then
                    mem.st.sol_by_tower = mem.st.sol_by_tower + 1
                end
            end
        end
    end
end

-- [ageofai tactics.lua M.bait, a port of upstream early.c tower_bait; GPL-3.0, used with Grille's permission] every
-- 2 s up to the Copper Age: a working citizen with an enemy soldier within 5 tiles (or one that hit it) and out of its
-- own towers' reach walks to the spot 1.5 tiles behind the nearest shooting building within 25 tiles from that soldier
local function bait(mem, events, now, bx, by, epoch)
    if (epoch or 0) > 2 or now - (mem.bait_t or -1000000) < 2000 then
        return
    end
    mem.bait_t = now
    local cov = {}
    for _, r in ipairs(REFUGES) do
        if r.tower then
            cov[#cov + 1] = r
        end
    end
    if #cov == 0 then
        return
    end
    local en = ai.seen{enemy = true, armed = true, category = "soldier", near = {bx, by, 36}, limit = 64}
    if #en == 0 then
        return
    end
    local ec = ai.get(en, {"x", "y"})
    local victim_of = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil then
            victim_of[e.unit] = e.attacker
        end
    end
    local cits = ai.units{category = "citizen", held = "engine", near = {bx, by, 32}, limit = 120}
    if #cits == 0 then
        return
    end
    local cx = ai.get(cits, {"x", "y", "action", "tx", "ty"})
    local sent = 0
    for k, id in ipairs(cits) do
        local ux, uy = cx.x[k], cx.y[k]
        if ux ~= nil and mem.away[id] == nil and sent < 12 then
            local hit = nil
            for j = 1, #en do
                local ex, ey = ec.x[j], ec.y[j]
                if ex ~= nil and (victim_of[id] == en[j] or (math.abs(ex - ux) <= 5 and math.abs(ey - uy) <= 5)) then
                    hit = j
                    break
                end
            end
            if hit ~= nil then
                local best, bd, covered = nil, 25 * 25, false
                for _, r in ipairs(cov) do
                    local d = d2(r.x, r.y, ux, uy)
                    if d * 4 <= (2 * r.range - 1) * (2 * r.range - 1) then
                        covered = true   -- (covered already: the tower fights)
                        break
                    end
                    if d < bd then
                        bd, best = d, r
                    end
                end
                if not covered and best ~= nil then
                    local ex, ey = ec.x[hit], ec.y[hit]
                    local dx, dy = best.x - ex, best.y - ey
                    local len = math.max(math.abs(dx), math.abs(dy), 1)
                    local sx, sy = best.x + dx * 3 // (2 * len), best.y + dy * 3 // (2 * len)
                    if sx == best.x and sy == best.y then
                        sx = best.x + (dx >= 0 and 1 or -1)
                    end
                    ai.move({id}, sx, sy, {forced = true})
                    local u = {id = id, x = ux, y = uy, action = cx.action[k], tx = cx.tx[k], ty = cx.ty[k]}
                    local a = go_away(mem, u, "bait", now)
                    a.t, a.bx, a.by = now, sx, sy
                    sent = sent + 1
                end
            end
        end
    end
    if sent > 0 then
        mem.st.bait = (mem.st.bait or 0) + sent
    end
end

-- [ageofai safety.lua] the live hurt spots within 6 tiles of a spot (a hit with the attacker in sight counts 20 s,
-- else 2 minutes)
local function hurt_count(mem, x, y, now)
    local n = 0
    for _, h in ipairs(mem.hurt) do
        local life = h.seen and 20000 or 120000
        if now - h.t <= life and math.abs(h.x - x) <= 6 and math.abs(h.y - y) <= 6 then
            n = n + 1
        end
    end
    return n
end

-- whether a site is dangerous by the hurt spots (a script's claims leave it alone meanwhile): mem.site_danger, kept
-- by safety()
function M.dangerous(mem, x, y, now)
    local d = mem.site_danger
    if d == nil then
        return false
    end
    for _, k in ipairs(ai.keys(d)) do
        if d[k].t <= now then
            d[k] = nil
        elseif math.abs(d[k].x - x) <= 6 and math.abs(d[k].y - y) <= 6 then
            return true
        end
    end
    return false
end

-- [ageofai safety.lua M.step, a port of upstream attack.c citizens_safety; GPL-3.0, used with Grille's permission] a
-- sixteenth of the gathering citizens every micro step (each looked at every 4 s): a citizen whose work spot is
-- raided (an enemy soldier within 6) or hurt (1 hurt spot within 6; 3 for the minerals) and not held by the town
-- goes to the nearest drop site, and the spot is dangerous for 30 s (raided) or 2 minutes (hurt)
local function safety(mem, now)
    if #mem.hurt == 0 and #CLUSTERS == 0 then
        return
    end
    local all = ai.units{category = "citizen", held = "engine", action = "gather"}
    if #all == 0 then
        return
    end
    local per = math.max(1, (#all + 15) // 16)
    local cursor = (mem.safety_k or 0) % #all
    local ids = {}
    for k = 1, per do
        ids[k] = all[(cursor + k - 1) % #all + 1]
    end
    mem.safety_k = (cursor + per) % #all
    local c = ai.get(ids, {"x", "y", "tx", "ty", "gathers", "action"})
    mem.site_danger = mem.site_danger or {}
    for k, id in ipairs(ids) do
        local sx, sy = c.tx[k] or c.x[k], c.ty[k] or c.y[k]
        if sx ~= nil and mem.away[id] == nil then
            local raid = raided(sx, sy, 6)
            local kind = c.gathers[k]
            local mineral = kind == "stone" or kind == "gold" or kind == "iron"
            local hurt = not raid and hurt_count(mem, sx, sy, now) >= (mineral and 3 or 1)
            if raid or hurt then
                -- the town holds it: own soldiers and towers within 8 beat the raiders within 8 by raid_held
                local foes = ai.seen{enemy = true, armed = true, category = "soldier", near = {sx, sy, 8}, limit = 20}
                local held = false
                if #foes > 0 then
                    local own = ai.units{armed = true, near = {sx, sy, 8}, limit = 40}
                    held = #own > 0 and ai.strength(own) * 100 >= ai.strength(foes) * M.opts.raid_held
                end
                if not held then
                    local did, _, dx, dy = ai.drop_site(sx, sy, mineral and "stone" or "food")
                    if did ~= nil and dx ~= nil and not (math.abs(dx - sx) <= 3 and math.abs(dy - sy) <= 3) then
                        ai.move({id}, dx, dy, {forced = true})
                        local a = go_away(mem, {id = id, x = c.x[k], y = c.y[k], action = c.action[k], tx = c.tx[k],
                                                ty = c.ty[k]}, "hurt", now)
                        a.t = now
                        mem.st.hurt_sent = (mem.st.hurt_sent or 0) + 1
                    end
                    local key = sx * 10000 + sy
                    mem.site_danger[key] = {x = sx, y = sy, t = now + (raid and 30000 or 120000)}
                end
            end
        end
    end
end

-- whether the script's claims should leave a site now (hurt_safety; false when it is off)
local LAST = nil
function M.site_blocked(x, y)
    return M.opts.hurt_safety and LAST ~= nil and M.dangerous(LAST, x, y, ai.time())
end

function M.micro(mem, events, now, tx, ty, epoch)
    LAST = mem
    mem.epoch = epoch
    state(mem)
    note(mem, events, now)
    if M.opts.hurt_safety and tx ~= nil and tx >= 0 then
        safety(mem, now)
    end
    if M.opts.bait and tx ~= nil and tx >= 0 then
        bait(mem, events, now, tx, ty, epoch)
    end
    if tx == nil or tx < 0 then
        return
    end
    if now - (mem.scan_t or -1000000) < 1000 then
        return
    end
    mem.scan_t = now
    for _, id in ipairs(ai.keys(mem.attackers)) do
        if now - mem.attackers[id] > M.opts.hostile_ms then
            mem.attackers[id] = nil
        end
    end
    scan(tx, ty)
    -- a raid: strong enough, or one of its units hit an own unit lately (a scout passing is none)
    local raids = {}
    for _, cl in ipairs(CLUSTERS) do
        local hostile = cl.hp * cl.atk >= M.opts.raid_min
        for k = 1, cl.n do
            hostile = hostile or mem.attackers[cl.ids[k]] ~= nil
        end
        if hostile then
            raids[#raids + 1] = cl
        end
    end
    CLUSTERS = raids
    -- the camps: where a raid has stood (a spot kept while a raid is within camp_r of it; forgotten 30 s after)
    mem.camps = mem.camps or {}
    for _, cl in ipairs(CLUSTERS) do
        local found = nil
        for _, c in ipairs(mem.camps) do
            if found == nil and d2(c.x, c.y, cl.x, cl.y) <= M.opts.camp_r * M.opts.camp_r then
                found = c
            end
        end
        if found == nil then
            found = {x = cl.x, y = cl.y, first = now}
            mem.camps[#mem.camps + 1] = found
        end
        found.last = now
        cl.camped = now - found.first
    end
    local keep = {}
    for _, c in ipairs(mem.camps) do
        if now - c.last <= 30000 and #keep < 16 then
            keep[#keep + 1] = c
        end
    end
    mem.camps = keep
    if #CLUSTERS > (mem.raids_now or 0) then
        mem.st.raids = mem.st.raids + #CLUSTERS - (mem.raids_now or 0)
    end
    mem.raids_now = #CLUSTERS
    for _, cl in ipairs(CLUSTERS) do
        handle(mem, cl, now)
    end
    returns(mem, now)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The strategy step: the tower repairs and the trace

local function repairs(mem, now)
    local o = M.opts
    local busy = {}
    for _, id in ipairs(ai.keys(mem.away)) do
        local a = mem.away[id]
        if a.why == "repair" and a.tower ~= nil then
            busy[a.tower] = (busy[a.tower] or 0) + 1
        end
    end
    local towers = 0
    for _, n in pairs(busy) do
        towers = towers + 1
    end
    for _, r in ipairs(REFUGES) do
        if r.tower and towers < o.repair_towers then
            local t = ai.unit(r.id)
            if t ~= nil and t.hp * 100 < t.hp_max * o.repair_pct and (busy[r.id] or 0) < o.repair_n then
                local want = o.repair_n - (busy[r.id] or 0)
                local pick = {}
                for _, id in ipairs(ai.units{category = "citizen", held = "engine", near = {t.x, t.y, 15}, limit = 12}) do
                    if #pick < want then
                        local u = ai.unit(id)
                        if u ~= nil and u.inside == nil and (u.action == "gather" or u.action == "idle")
                                and not raided(u.x, u.y, o.danger_r) then
                            pick[#pick + 1] = u
                        end
                    end
                end
                if #pick > 0 then
                    local ids = {}
                    for _, u in ipairs(pick) do
                        ids[#ids + 1] = u.id
                    end
                    if ai.repair(ids, r.id) == "ok" then
                        for _, u in ipairs(pick) do
                            local a = go_away(mem, u, "repair", now)
                            a.tower = r.id
                        end
                        towers = towers + 1
                        mem.st.repair = mem.st.repair + #ids
                    end
                end
            end
        end
    end
end

function M.step(view, mem)
    state(mem)
    if M.opts.repair and view.time - (mem.repair_t or -1000000) >= 5000 then
        mem.repair_t = view.time
        repairs(mem, view.time)
    end
    if view.time % 300000 < 1000 then
        local s = mem.st
        local away = #ai.keys(mem.away)
        print(string.format("def stats raids=%d hits=%d fled=%d back=%d militia=%d repair=%d away=%d lost_cit=%d lost_sol=%d sol_by_tower=%d",
            s.raids, s.hits, s.fled, s.back, s.militia, s.repair, away, s.lost_cit, s.lost_sol, s.sol_by_tower))
    end
end

return M

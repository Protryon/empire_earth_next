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
                          -- citizens: seed 4 against the classic lost a citizen in each such fight, odds or not)
    tower_share = 100,    -- percent of a tower's attack per 2 s of its reload counted in the odds
    clear_r = 10,         -- a spot is clear with no raider this near ...
    clear_ms = 8000,      -- ... and no citizen hit near it for this long
    away_min = 6000,      -- a citizen stays away at least this long
    away_max = 150000,    -- and at most this long (then the engine has it again)
    reflee_ms = 3000,     -- a citizen still in danger is sent again after this long
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
    for _, t in ipairs({"b_town_center", "b_capitol"}) do
        for _, id in ipairs(ai.units{type = t, built = true, limit = 4}) do
            local u = ai.unit(id)
            if u ~= nil then
                REFUGES[#REFUGES + 1] = {id = id, x = u.x, y = u.y, tower = false, range = 0}
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
        if raider_d2(cl, x, y) <= rr * rr then
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
local function refuge_for(x, y, rx, ry, ship)
    local best, bd = nil, nil
    local dc = d2(x, y, rx, ry)
    for _, r in ipairs(REFUGES) do
        local dr = d2(r.x, r.y, rx, ry)
        if (r.tower and not ship) or dr >= dc then   -- (from a warship: away from it, a tower outranged)
            local d = d2(x, y, r.x, r.y)
            if bd == nil or d < bd then
                best, bd = r, d
            end
        end
    end
    if best == nil then
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
    for _, id in ipairs(ai.keys(mem.away)) do
        local a = mem.away[id]
        local u = ai.unit(id)
        if u == nil then
            mem.away[id] = nil
        else
            local done = false
            if a.why == "militia" then
                done = now >= a.t + o.militia_ms or not raided(u.x, u.y, o.near_r)
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
    if not held and cl.n <= o.militia_max and (o.militia_ranged or cl.ranged == 0)
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
                local fx, fy = refuge_for(u.x, u.y, cl.x, cl.y, cl.ship)
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
                mem.hurt[k] = {x = v.x, y = v.y, t = now}
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

function M.micro(mem, events, now, tx, ty)
    state(mem)
    note(mem, events, now)
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

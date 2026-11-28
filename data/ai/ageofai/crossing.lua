-- Age of AI port: upstream dll/ai/crossing.c (THE CROSSING: what a transport risks on its way over and the way it
-- takes, the meeting point, the places the transports were sunk; UPSTREAM names the commit, the per-file map and the
-- licence - GPL-3.0, Grille and the Age of AI contributors). require("ageofai.crossing").
--
-- The player, 2026-11-18 (crossing.c's header): "the safety check should be smart, and plan its route based on what it
-- knows and how important the delivery is - 1. is it ok to lose the transport as long the delivery is made? (invasions
-- or defensive reinforcements, not regular army or citizens balancing) 2. how dense is the enemy coverage? will it get
-- stuck and sunk before it can deliver? 3. are the enemy ships already bound/distracted in a battle with an own fleet?"
-- -- and yes to the rest: fog and old knowledge, a fuller load into danger, the places transports were sunk, storms,
-- aircraft and submarines.
--
-- THE THREATS (M.scan, every CX_LOOK per transport type): whatever hits the transport type - the enemy ships,
-- submarines and aircraft it sees now; the defences and the long guns on a coast it has seen (the memory's buildings,
-- until their spot is looked at again); the ships it remembers (ai.memory, the engine's memory_ms - 3 minutes - where
-- upstream keeps 5) where it saw them, the reach grown by CX_DRIFT for the time since and the damage counted half; the
-- enemy storms it sees now (the same units storms.lua hunts, by their names: no dependency on storms.lua's look); and
-- the places its transports were sunk in the last CX_SUNK_T (M.sunk). A ship within its reach of a warship of its own
-- is BUSY: in a fight with its fleet, counted a quarter.
--
-- THE RATING (M.risk): every 2 tiles of the way the time the transport gets there at its speed; a ship or an aircraft
-- goes for it from the first point that passes within its sight (CX_NOTICE beyond its reach) and hits it from when it
-- has closed in - to the drop and through the unload when it is as fast as the transport, else for as long as the
-- transport takes to pass its reach; a standing one (a tower, a storm, a place of losses) while the way lies in its
-- reach. Its damage a second times those seconds, summed: the damage expected before the load is off, a share of the
-- transport's hit points. THE RISK IS IN HUNDREDTHS (100 = the transport's whole hit points), as upstream's float risk
-- x 100: the engine validates the script's memory and takes integers only.
--
-- THE WAY (M.risk gives it): the straight one and through the points CX_SIDE tiles off its middle to either side (twice
-- that too), the sea's water nearest them, the least risk (a minute's sailing counts as CX_TIME of it) - the transport
-- goes through that point first. THE MEETING POINT (M.meet, cx_meet): where a wave's transports wait for each other off
-- the water they unload at - 10 to 18 tiles out towards the way they came, 8 to either side, the least damage a second
-- a waiting transport would take of what it knows.
--
-- THE DECISION (M.tolerance, read by the ferry): an important delivery - soldiers for a land under attack, later a push
-- across the water - goes while it has a fair chance to arrive (under CX_GO_HIGH: the transport may be lost after); any
-- other - the shares, the citizens - only while it is likely to come through (CX_GO_LOW, CX_GO_STEP more each minute
-- it has waited, up to CX_GO_WAIT); into danger (over CX_FULL) with a full load only (ferry.lua). A trip that may not go
-- waits off the shore, unloaded, its escort asked for meanwhile (convoy.c: transport_escort, not ported).
--
-- CALIBRATED (2026-11-18, crossing.c): the rating is a rank, not the share it says - by band the trips delivered, those
-- put off on a beach under fire or sunk, the transports lost before they sailed: under 0.05 62% delivered, 20% of the
-- sailed in trouble, 8% lost before; 0.2-1 45%, about 35%, 12%; 1-2.5 31%, 47%, 28%; 2.5-5 20%, 66%, 30%; over 5 7%,
-- 80%, 50%. MEASURED (mirrored, 12 seeds): as much delivered for less than half the transports lost: on.
--
-- ENGINE GAPS (what upstream reads and this engine cannot): ld_factor (the game's hit-factor table) is fleet.lua's
-- role table for a warship and ai.can_hurt (again the game's weapon-family table) for every other attacker - ai.damage's
-- row of docs/AI_LUA_UNITS.md §2.2 is not built; the unit's own upgraded range, attack and speed are the class data's
-- (ai.class reads the reader's tree; the unit record has speed, range and sight but not attack and reload); fl_danger_r
-- (a coast gun's reach) stands as any land unit's range of 5+ tiles; fl_water (the sea's water nearest a point) is a
-- ring of ai.tile of its own; the storms are the ones it sees now (no storms.lua look); a remembered ship lives as long
-- as ai.memory's memory_ms (3 minutes, upstream 5, CX_DRIFT grows its reach for the time). The threat list (upstream's
-- CX_N 4096 in the AI's globals) is a module local here, capped at M.N and rebuilt every CX_LOOK, so it is no part of
-- the save and a load scans again: nothing of it is a decision that has to survive.
local F = require("ageofai.fleet")
local M = {}

-- the constants (crossing.c)
M.LOOK = 2000            -- CX_LOOK: the threats looked at this often per AI (and per transport type)
M.N = 512                -- CX_N 4096 upstream: the list (a module local: a few hundred at most here, see the header)
M.DRIFT_MS = 2000        -- CX_DRIFT 0.5 tiles a second a remembered ship may have sailed ...
M.DRIFT_MAX = 12         -- ... at most this many (CX_DRIFT_MAX)
M.SUNK_T = 300000        -- CX_SUNK_T: a place its transports were sunk counts this long
M.SUNK_R = 8             -- CX_SUNK_R (tiles)
M.SUNK_N = 8             -- the sunk places it remembers
M.SIDE = 10              -- CX_SIDE: the ways off the line, this many tiles to a side, and twice that
M.TIME = 10              -- CX_TIME (0.1 of risk a minute's sailing, x100)
M.UNLOAD = 8000          -- CX_UNLOAD: seconds an unload takes (ms)
M.NOTICE = 8             -- CX_NOTICE: a ship goes for a transport that passes this far beyond its reach
M.GO_HIGH = 500          -- CX_GO_HIGH 5.0
M.GO_LOW = 100           -- CX_GO_LOW 1.0
M.GO_WAIT = 250          -- CX_GO_WAIT 2.5
M.GO_STEP = 25           -- CX_GO_STEP 0.25 a minute of waiting
M.FULL = 35              -- CX_FULL 0.35: into danger with a full load only
M.FLEET_R = 16           -- ferry.c FY_WAY_R: the water its fleet holds for a way (M.cleared)
M.STORM_R = 5            -- storms.c ST_R: a storm's reach
M.SAMPLES = 256          -- the way's points at most (upstream's vec2 pt[256])

-- the storm units (storms.c is_storm): the prophet's hurricane and the Anti-Matter Storm
local STORM = {hurricane = true, anti_matter_storm = true}

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

-- the hit factor of a weapon against a transport type, in percent: a warship's is fleet.lua's role table (this
-- engine's stand-in for the game's damage-class table); anything else that can hit it - a defence, a coast gun, an
-- aircraft, a storm's neighbour - is ai.can_hurt's weapon family (the game's own table), a plain hit. Upstream's
-- ld_factor reads the game's table for every pair; ai.damage's row of docs/AI_LUA_UNITS.md §2.2 is not built.
-- Static, so cached per pair.
local HAVE_DAMAGE = ai.damage ~= nil
local HAVE_HURT = ai.can_hurt ~= nil
local factors = {}
local function factor(at, tt)
    local key = at .. ">" .. tt
    local f = factors[key]
    if f == nil then
        if HAVE_DAMAGE then
            f = ai.damage(at, tt)
        end
        if f == nil then
            if F.role(at) ~= 0 then
                f = F.factor(at, tt)
            elseif HAVE_HURT then
                f = ai.can_hurt(at, tt) and 100 or 0
            else
                f = 0
            end
        end
        factors[key] = f
    end
    return f
end

-- ---------------------------------------------------------------------------------------------------------------
-- The threat list (cx_scan): a module local, rebuilt every CX_LOOK (no memory block: it is no decision, and a load
-- scans again). Parallel arrays: x, y, r (tiles), dps (milli-hit-points a second), v (centi-tiles a second, 0
-- standing), air, busy (in a fight with its fleet), old (remembered or a place of losses)

local L = {t = -1, tt = false, n = 0, x = {}, y = {}, r = {}, dps = {}, v = {}, air = {}, busy = {}, old = {},
           top = 0, top_dmg = 0, dropped = 0}

local function add(ex, ey, er, edps, ev, eair, eold)
    if er <= 0 or edps <= 0 then
        return
    end
    if L.n >= M.N then
        L.dropped = L.dropped + 1
        return
    end
    local k = L.n + 1
    L.n = k
    L.x[k], L.y[k], L.r[k], L.dps[k] = ex, ey, er, edps
    L.v[k], L.air[k], L.busy[k], L.old[k] = ev or 0, eair == true, false, eold == true
end

-- the threats to transports of type ttype as the AI knows them now (cx_scan); kept for CX_LOOK
function M.scan(view, memory, ttype)
    ttype = ttype or ""
    local now = view.time
    if L.t >= 0 and now >= L.t and now - L.t < M.LOOK and L.tt == ttype then
        return
    end
    L.t = now
    L.tt = ttype
    L.n = 0
    L.dropped = 0
    L.top, L.top_dmg = 0, 0
    local tcls = F.cls(ttype)
    -- its warships: what they fight is busy; their reach
    local ox, oy, orr, no = {}, {}, {}, 0
    local wars = ai.units{category = "ship", armed = true, limit = 256}
    if #wars > 0 then
        local wc = ai.get(wars, {"x", "y", "type"})
        for k = 1, #wars do
            if wc.x[k] ~= nil then
                no = no + 1
                ox[no], oy[no] = wc.x[k], wc.y[k]
                orr[no] = (F.cls(wc.type[k]).range or 0) // 100
            end
        end
    end
    -- what it sees now: the storms, the ships, submarines, aircraft, the defences, the long guns on a coast
    local ids = ai.seen{enemy = true}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "type", "category"})
        for k = 1, #ids do
            local ex, ey, et = cols.x[k], cols.y[k], cols.type[k]
            if ex ~= nil and et ~= nil then
                local c = F.cls(et)
                if STORM[et] then
                    add(ex, ey, M.STORM_R, 100000, 0, false, false)   -- 100 a second to its owner's enemies
                elseif (c.attack or 0) > 0 then
                    local f = factor(et, ttype)
                    if f > 0 then
                        local cat = cols.category[k]
                        local d = F.dps(et) * f // 100
                        if cat == "building" then
                            local rr = (c.range or 0) // 100
                            if rr > 0 then
                                add(ex, ey, rr, d, 0, false, false)
                            end
                        elseif cat == "air" then
                            add(ex, ey, math.max((c.range or 0) // 100, 1), d, c.speed or 0, true, false)
                        elseif cat == "ship" then
                            add(ex, ey, math.max((c.range or 0) // 100, 1), d, c.speed or 0, false, false)
                        else   -- a land unit with a long gun on the coast (fleet.c fl_danger_r's rule)
                            local rr = (c.range or 0) // 100
                            if rr >= 5 then
                                add(ex, ey, rr, d, 0, false, false)
                            end
                        end
                    end
                end
            end
        end
    end
    -- the defences it has seen (the memory's buildings: until their spot is looked at again); those it sees now are
    -- in the list above already
    for _, rec in ipairs(ai.memory{buildings = true}) do
        if rec.x ~= nil and not (rec.seen ~= nil and now - rec.seen <= 1000) then
            local c = F.cls(rec.type)
            local f = factor(rec.type, ttype)
            if f > 0 and (c.attack or 0) > 0 then
                local rr = (c.range or 0) // 100
                if rr > 0 then
                    add(rec.x, rec.y, rr, F.dps(rec.type) * f // 100, 0, false, false)
                end
            end
        end
    end
    -- the ships it remembers: where it saw them, the reach grown by the drift, the damage counted half (intel.c)
    local mobiles = ai.memory{since = math.max(now - M.SUNK_T, 0)}
    for _, rec in ipairs(mobiles) do
        if not rec.building and rec.x ~= nil then
            local kind = F.kind(rec.type)
            if kind == "ship" or kind == "siege" or kind == "other" then
                local c = F.cls(rec.type)
                local f = factor(rec.type, ttype)
                if f > 0 and (c.attack or 0) > 0 then
                    local age = now - (rec.seen or now)
                    local drift = age // M.DRIFT_MS
                    if drift > M.DRIFT_MAX then
                        drift = M.DRIFT_MAX
                    end
                    local rr = (c.range or 0) // 100
                    local on_land = kind ~= "ship"
                    if (on_land and rr >= 5) or kind == "ship" then
                        add(rec.x, rec.y, math.max(rr, 1) + drift, F.dps(rec.type) * f // 200, 0, false, true)
                    end
                end
            end
        end
    end
    -- the places its transports were sunk lately: as if something sank them there still
    local cx = memory.crossing
    if cx ~= nil and cx.sunk ~= nil then
        for k = 1, M.SUNK_N do
            local s = cx.sunk[k]
            if s ~= nil and (s.t or 0) > 0 and s.t <= now and now - s.t < M.SUNK_T then
                add(s.x, s.y, M.SUNK_R, (tcls.hp or 0) * 50, 0, false, true)   -- hp / 20 a second
            end
        end
    end
    -- BUSY: within its reach of a warship of its own (and so in its fight)
    for i = 1, L.n do
        if L.v[i] > 0 and not L.air[i] then
            for j = 1, no do
                local rr = math.max(L.r[i], orr[j]) + 2
                if d2(L.x[i], L.y[i], ox[j], oy[j]) <= rr * rr then
                    L.busy[i] = true
                    break
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The rating (cx_way)

-- the damage expected on the way a -> (via) -> b at speed vtc (centi-tiles a second) for a transport of hp, through
-- the unload at b, as milli-hit-points; returns the sailing time in ms. The way's worst threat is left in L.top.
local function way_damage(ax, ay, vx, vy, bx, by, vtc, hp, ocean)
    local px, py, tm = {}, {}, {}
    local lx = {ax, vx ~= nil and vx or bx, bx}
    local ly = {ay, vy ~= nil and vy or by, by}
    local nl = vx ~= nil and 3 or 2
    local n, t = 0, 0
    for l = 1, nl - 1 do
        local x0, y0, x1, y1 = lx[l], ly[l], lx[l + 1], ly[l + 1]
        local dx, dy = x1 - x0, y1 - y0
        local len = isqrt(dx * dx + dy * dy)
        local steps = len // 2 + 1
        local leg = len * 100000 // vtc
        for i = (l == 1 and 0 or 1), steps do
            if n >= M.SAMPLES then
                break
            end
            n = n + 1
            px[n] = x0 + dx * i // steps
            py[n] = y0 + dy * i // steps
            tm[n] = t + leg * i // steps
        end
        t = t + leg
    end
    if n == 0 then
        L.top, L.top_dmg = 0, 0
        return 0, 0
    end
    local endt = t + M.UNLOAD
    local x0, x1, y0, y1 = ax, ax, ay, ay
    for l = 2, nl do
        if lx[l] < x0 then x0 = lx[l] end
        if lx[l] > x1 then x1 = lx[l] end
        if ly[l] < y0 then y0 = ly[l] end
        if ly[l] > y1 then y1 = ly[l] end
    end
    local dmg, top, top_dmg = 0, 0, 0
    for i = 1, L.n do
        local er, edps, ev = L.r[i], L.dps[i], L.v[i]
        local reach = er + (ev > 0 and M.NOTICE or 0)
        if not (L.x[i] < x0 - reach or L.x[i] > x1 + reach or L.y[i] < y0 - reach or L.y[i] > y1 + reach) then
            local ok = true
            if ev > 0 and not L.air[i] and ocean ~= nil and ocean ~= 0 then
                local et = ai.tile(L.x[i], L.y[i])
                ok = et ~= nil and et.sea == ocean   -- a ship on another sea
            end
            if ok then
                local w = L.busy[i] and 25 or 100
                local exp_ms = 0
                if ev <= 0 then   -- standing: while the way lies in its reach, and the unload if it ends there
                    for k = 1, n do
                        if d2(px[k], py[k], L.x[i], L.y[i]) <= er * er then
                            exp_ms = exp_ms + (k > 1 and tm[k] - tm[k - 1] or 0) + (k == n and M.UNLOAD or 0)
                        end
                    end
                else   -- moving: from the first point within its sight - as fast, it follows; slower, while it passes
                    for k = 1, n do
                        local d = isqrt(d2(px[k], py[k], L.x[i], L.y[i])) - er
                        if d <= M.NOTICE then
                            local close = d > 0 and d * 100000 // ev or 0
                            local pass = 2 * er * 100000 // vtc
                            local left = endt - tm[k] - close
                            if left > 0 then
                                exp_ms = ev * 10 >= vtc * 9 and left or math.min(pass, left)
                            end
                            break
                        end
                    end
                end
                local d = edps * exp_ms // 1000 * w // 100
                dmg = dmg + d
                if d > top_dmg then
                    top, top_dmg = i, d
                end
            end
        end
    end
    L.top, L.top_dmg = top, top_dmg
    return dmg, t
end

-- the risk of dmg milli-hit-points against a transport of hp, in hundredths
local function risk_of(dmg, hp)
    if hp <= 0 then
        return 0
    end
    return dmg * 100 // (hp * 1000)
end

-- the sea's water nearest a point, within rad tiles (fl_water): nil when there is none
function M.water_near(x, y, oc, rad)
    if oc == nil or oc == 0 then
        return nil
    end
    rad = rad or 4
    for r = 0, rad do
        for dy = -r, r do
            for dx = -r, r do
                if r == 0 or dx == -r or dx == r or dy == -r or dy == r then
                    local t = ai.tile(x + dx, y + dy)
                    if t ~= nil and t.sea == oc then
                        return x + dx, y + dy
                    end
                end
            end
        end
    end
    return nil
end

-- the way for a transport from the water a to the water b on sea ocean, its risk (hundredths of its hit points) and
-- the point off the line it goes through first: -> risk, via_x, via_y, has_via, secs (ms). opts: type, speed (centi a
-- second), hp, ocean; kind is the ferry's trip kind (upstream's crossing_rate takes none: unused by the rating).
function M.risk(view, memory, ax, ay, bx, by, kind, opts)
    opts = opts or {}
    local vtc = opts.speed or 75
    if vtc < 30 then
        vtc = 30
    end
    local hp = opts.hp or 1000
    local ocean = opts.ocean
    M.scan(view, memory, opts.type)
    local cx = memory.crossing
    if cx ~= nil then
        cx.rated = (cx.rated or 0) + 1
        cx.dropped = (cx.dropped or 0) + L.dropped
        L.dropped = 0
    end
    local best, secs = way_damage(ax, ay, nil, nil, bx, by, vtc, hp, ocean)
    local top, top_dmg = L.top, L.top_dmg
    local best_score = risk_of(best, hp) + secs // 6000   -- CX_TIME: a minute's sailing counts as 0.1 of risk
    local vx, vy, hv = 0, 0, false
    if best > 0 then
        local dx, dy = bx - ax, by - ay
        local len = isqrt(dx * dx + dy * dy)
        if len >= 4 then
            local mx, my = (ax + bx) // 2, (ay + by) // 2
            for s = -2, 2 do
                if s ~= 0 then
                    local qx = mx + (-dy * (M.SIDE * s)) // len
                    local qy = my + (dx * (M.SIDE * s)) // len
                    local wx, wy = M.water_near(qx, qy, ocean, 6)
                    if wx ~= nil then
                        local d, sc = way_damage(ax, ay, wx, wy, bx, by, vtc, hp, ocean)
                        local score = risk_of(d, hp) + sc // 6000
                        if score < best_score then
                            best_score, best, secs = score, d, sc
                            vx, vy, hv = wx, wy, true
                            top, top_dmg = L.top, L.top_dmg
                        end
                    end
                end
            end
        end
    end
    L.top, L.top_dmg = top, top_dmg
    if hv and cx ~= nil then
        cx.detours = (cx.detours or 0) + 1
    end
    return risk_of(best, hp), vx, vy, hv, secs
end

-- where a wave's transports wait for each other off the water b they unload at, coming from a (cx_meet): open water
-- of the sea ocean 10-18 tiles out towards a, 8 to either side too, the least damage a second a transport waiting
-- there would take of what it knows. ttype: the transport type the threats are rated for. nil: b is near enough -
-- they come together anyway
function M.meet(view, memory, ax, ay, bx, by, ocean, ttype)
    M.scan(view, memory, ttype)
    local dx, dy = ax - bx, ay - by
    local len = isqrt(dx * dx + dy * dy)
    if len < 20 then
        return nil
    end
    local best, bx_, by_ = nil, nil, nil
    for r = 10, 18, 4 do
        for s = -1, 1 do
            local qx = bx + (dx * r) // len - (dy * (8 * s)) // len
            local qy = by + (dy * r) // len + (dx * (8 * s)) // len
            local wx, wy = M.water_near(qx, qy, ocean, 4)
            if wx ~= nil and d2(wx, wy, bx, by) >= 64 then
                local dps = 0
                for i = 1, L.n do
                    local on = true
                    if L.v[i] > 0 and not L.air[i] and ocean ~= nil and ocean ~= 0 then
                        local et = ai.tile(L.x[i], L.y[i])
                        on = et ~= nil and et.sea == ocean   -- a ship on another sea
                    end
                    if on then
                        local reach = L.r[i] + (L.v[i] > 0 and M.NOTICE or 0)
                        if d2(wx, wy, L.x[i], L.y[i]) <= reach * reach then
                            dps = dps + L.dps[i] * (L.busy[i] and 25 or 100) // 100
                        end
                    end
                end
                local sc = dps // 10 + r * 10 + (s ~= 0 and 1 or 0)   -- alike: the nearer, on the line
                if best == nil or sc < best then
                    best, bx_, by_ = sc, wx, wy
                end
            end
        end
    end
    return bx_, by_
end

-- whether the water at (x, y) is the AI's now: no enemy warship within FY_WAY_R of it, or its own warships there
-- outweigh them (the port's stand-in for the squadrons' rating of a way - squadrons.c sq_sea_value/sea_first)
function M.cleared(view, memory, x, y)
    local foes = ai.seen{enemy = true, category = "ship", near = {x, y, M.FLEET_R}, limit = 8}
    if #foes == 0 then
        return true
    end
    local own = ai.units{category = "ship", armed = true, near = {x, y, M.FLEET_R}, limit = 16}
    if #own == 0 then
        return false
    end
    return ai.strength(own) >= ai.strength(foes)
end

-- a transport of its sunk at (x, y): kept in the 8 places, the oldest gone (cx_sunk)
function M.sunk(memory, x, y, now)
    memory.crossing = memory.crossing or {sunk = {}}
    local c = memory.crossing
    c.sunk = c.sunk or {}
    local k = 1
    for j = 1, M.SUNK_N do
        local s = c.sunk[j]
        if s == nil then
            k = j
            break
        end
        if (s.t or 0) < (c.sunk[k].t or 0) then
            k = j
        end
    end
    c.sunk[k] = {x = x, y = y, t = (now or 0) > 0 and now or 1}
end

-- THE DECISION: the risk a trip of kind may take now - an important one (imp) arriving before it is sunk, any other
-- coming through, more the longer none of its kind went for the risk (fy_tolerance; risky is the ferry's per kind)
function M.tolerance(risky, kind, imp, now)
    if imp then
        return M.GO_HIGH
    end
    local tol = M.GO_LOW
    local t = risky ~= nil and risky[kind] or 0
    if t > 0 and now >= t then
        tol = tol + (now - t) // 60000 * M.GO_STEP
    end
    return tol < M.GO_WAIT and tol or M.GO_WAIT
end

-- a new game or a load: the last game's sunk places would come back as threats at the old map's coordinates
function M.new_game(memory)
    memory.crossing = {sunk = {}}
end

-- the log's word on the worst threat of the last rating (cx_top_text)
function M.top()
    local i = L.top
    if i < 1 or i > L.n then
        return "none"
    end
    return string.format("%d damage from %s at %d,%d (reach %d, %d a second, speed %.1f%s%s)",
        L.top_dmg, L.air[i] and "an aircraft" or L.v[i] > 0 and "a ship" or L.old[i] and "a remembered ship or a loss's place" or "a defence or storm",
        L.x[i], L.y[i], L.r[i], L.dps[i] // 1000, L.v[i] / 100.0, L.busy[i] and ", busy" or "",
        L.old[i] and ", remembered" or "")
end

-- the rating's counters (crossing.c's cx_st, the minute log's)
function M.stats(memory)
    local c = memory.crossing
    return c ~= nil and c or {}
end

return M

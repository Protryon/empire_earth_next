-- Age of AI port: upstream dll/ai/squadrons.c (the warships as several squadrons instead of one fleet; UPSTREAM has the
-- map and the licence: GPL-3.0, Grille and the Age of AI contributors). require("ageofai.squadrons").
-- ageofai.fleet hands its ships over when the switch is on (fleet.c fleet_tick's sq_tick), and takes them back when it
-- goes off.
--
-- The player, 2026-11-17: "water needs a rework i think, the ai has a single fleet, it dosnt try to hold a sea i think,
-- just move its single blob" - and of how he plays it: "small scout and raid groups, and one main fleet where its
-- needed ... multiple fleets that can hold on their own for a bit until the other fleets come for help if needed".
-- So, by sea (the game's connected water):
--   ZONES (every SQ_ZONES): the sea territories that matter to it - its coast along land it holds, the fish it has seen
--     there, enemy warships seen there lately (doubled); the most valuable anchors a zone, every such water within
--     SQ_REACH of it is the zone's, the next uncovered one anchors the next zone. As many main squadrons as zones and
--     ships allow (SQ_MAIN_MIN each), the most valuable zones first.
--   SHIPS: frigates first to the scout groups (SQ_SCOUT_N each, two groups with two zones), the rest to the main
--     squadrons by their zones' worth; a ship keeps its squadron, one is moved a look while a squadron is 2+ over its
--     share and another 2+ under.
--   EACH SHIP (every fleet look, 2 s): one in a danger zone out of it at once, one whose target is in one - or, THE
--     SHORE, a land target out of its reach - back to its squadron's point, a hurt one (under SQ_REPAIR of its hit
--     points) to the nearest dock out of danger until 80%, under fire one that is hurt waits behind the nearest main
--     squadron, away from the chasers.
--   MAIN - at rest at its zone's anchor, out of every danger zone: enemy warships in its zone fought when clearly the
--     stronger (SQ_ENGAGE) and on while not losing (SQ_HOLD), else it holds under its own towers (or by its dock) and
--     calls for help; a quiet main squadron with half its hit points answers, until the caller's zone is quiet again.
--     Quiet and clearly stronger (SQ_PUSH) than the enemy warships within 2 x SQ_REACH: it goes after them, back at 1.1.
--   HUNT - the submarines and galleys (both hit battleships hard and lose to frigates), SQ_HUNT_N at most: after the
--     nearest enemy warship within SQ_HUNT_R that no enemy frigate is near and whose company they beat; an enemy frigate
--     near themselves: away to the nearest main squadron.
--   HOME - enemy warships within SQ_HOME_R of one of its docks or of its fishing boats are the nearest main squadron's.
--   SCOUT - the waters of its sea not seen longest (the unheld ones first), raiding enemy fishing boats and transports
--     within SQ_RAID_R where no enemy warship near them stands a fight with it, running from warships within
--     SQ_SCOUT_FLEE it doesn't beat as clearly (SQ_SCOUT_FIGHT): raiders, not fighters.
--   STRIKE - the missile submarines (no ship to hit, buildings and land units from 24 tiles) with SQ_STRIKE_FRIG
--     frigates and a cruiser: out where the AI sees enemy troops or buildings near the water (each worth its hit
--     points, towers and siege x3, buildings x0.5), the most worth in a place at least SQ_STRIKE_MIN; back to the main
--     squadron from warships it doesn't beat.
--   THE SHORE (sea_shore_fire): the quiet moves are plain moves; a main squadron at rest with SQ_SHELL_HP of its hit
--     points shells the enemy buildings it sees on its sea's coast within SQ_SHELL_R of its zone, the nearest first,
--     only when its estimate (Lanchester's square law both ways) beats the defences that reach the water there - from
--     SQ_SHELL_GO, held while SQ_SHELL_HOLD, the defences first.
--   CONCENTRATION: on a sea where all its warships do not beat all the enemy's it knows there (SQ_SPLIT, together under
--     1.0) there is one main squadron.
-- A "squadron" is a lib.groups manual group (the lib keeps the members, drops the dead, keeps the centre and the
-- strength; this module gives every order). The storms are in the fleet's scan's danger zones (storms.lua): nobody is
-- sent into one, and care() takes out those inside.
--
-- OFF IN THE PORT: the switch is settings.sea_squadrons (upstream ships it on), but the port ships it off - M.PORTED
-- below is the port's own gate, so a squadron path that cannot be measured here never runs by accident. Turn both on
-- together once it has been played (the report says so).
--
-- Missing (the UPSTREAM line names them too): the escort of the ferries' transports and of a push's landing (convoy.c,
-- crossing.c and stations.c are not ported), the kill and loss counters by squadron and by what it did (the port's
-- events carry no target), the claims' priority (one squadron owns a ship, nobody else may order it meanwhile), and
-- missile subs are trained by navy.lua (the port's mix has no missile-sub role: a Trident hits no ship).
local G = require("lib.groups")
local S = require("ageofai.settings")
local T = require("ageofai.territories")   -- [territories.c] THE SEAS: the zones' worth, the fish, the threats
local F = require("ageofai.fleet")         -- [fleet.c] the scan, the danger zones, the fight estimate, the water
local M = {}

M.PORTED = false   -- the port's gate: settings.sea_squadrons AND this (see the header)

local SQ_MAX = 8
local SQ_MAIN_MIN = 5
local SQ_REACH = 40
local SQ_ZONES = 60000
local SQ_LOOK = 4000
local SQ_SCOUT_N = 3
local SQ_HUNT_N = 3
local SQ_HUNT_R = 50
local SQ_FRIG_R = 12
local SQ_RAID_R = 40
local SQ_SCOUT_FIGHT = 200   -- percent: a fight only this clearly the stronger
local SQ_SCOUT_FLEE = 16     -- warships so near looked at
local SQ_HOLD = 100          -- percent, the fight estimate at which it stays
local SQ_ENGAGE = 130        -- ... from which it fights in its zone
local SQ_PUSH = 150          -- ... from which it goes after them
local SQ_SPLIT = 130         -- the whole sea's ratio from which it splits into zones (together under 1.0)
local SQ_REPAIR = 50         -- percent of its hit points: to the dock
local SQ_BEHIND = 10         -- so far past the main squadron's middle a hurt ship under fire waits
local SQ_BEHIND_E = 12       -- ... under fire: an enemy warship seen so near
local SQ_BEHIND_M = 30       -- ... a main squadron so near
local SQ_HOME_R = 15
local SQ_SHELL_HP = 70       -- percent of its hit points from which a main squadron shells
local SQ_SHELL_OFF = 40      -- ... and under which it stops
local SQ_SHELL_R = 30        -- the coast's buildings so far from its zone
local SQ_SHELL_DEF = 4       -- a defence's reach + this takes in a building
local SQ_SHELL_GO = 150      -- the estimate from which it shells (percent)
local SQ_SHELL_HOLD = 110    -- ... and under which it stops
local SQ_SHELL_LOOKS = 6
local SQ_STRIKE_FRIG = 2     -- a strike group's frigates, and a cruiser
local SQ_STRIKE_R = 70
local SQ_STRIKE_RANGE = 22
local SQ_STRIKE_AREA = 5
local SQ_STRIKE_MIN = 600
local SQ_STRIKE_FLEE = 14
local SQ_STRIKE_N = 48
local SQ_MSUB_LOOK = 10000   -- the missile subs' own scan (the fleet's leaves them out)

local ROLE_M = 6             -- the missile submarines (no role in the fleet's mix: no ship to hit)
local MAIN, SCOUT, HUNT, STRIKE = "main", "scouts", "hunters", "strike"

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
local function is_msub(t)
    return t ~= nil and (t:find("trident") or t:find("triton")) ~= nil
end
local function hunter(role)
    return role == F.SUB or role == F.ANTI
end
-- what a squadron of kind k takes (main: all but the hunters; scouts: frigates; hunters: submarines and galleys; strike:
-- the missile subs and their escort)
local function fits(kind, role)
    if kind == HUNT then
        return hunter(role)
    elseif kind == SCOUT then
        return role == F.FRIG
    elseif kind == STRIKE then
        return role == ROLE_M or role == F.FRIG or role == F.AA
    end
    return not hunter(role) and role ~= ROLE_M
end
local function ids_of(q)
    local out = {}
    for _, u in ipairs(q.u) do
        out[#out + 1] = u.id
    end
    return out
end
local function centre_of(q)
    local x, y, n = 0, 0, 0
    for _, u in ipairs(q.u or {}) do
        x, y, n = x + u.x, y + u.y, n + 1
    end
    if n == 0 then
        return q.zc_x or -1, q.zc_y or -1
    end
    return x // n, y // n
end
-- the hit points a squadron has, percent
local function hp_share(q)
    local h, m = 0, 0
    for _, u in ipairs(q.u) do
        h, m = h + u.hp, m + u.hp_max
    end
    return m > 0 and h * 100 // m or 0
end
-- the fight estimate (fleet.c fl_ratio) as percent, 99 x 100 when nothing hurts it
local function ratio_of(q, foes)
    return math.floor(math.min(F.ratio_of(q.u, foes), 99) * 100)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The state, the groups and the membership

function M.on()
    return M.PORTED and S.sea_squadrons == true
end

-- the fleet hands the ships over (nil memory or no sea: it keeps them)
function M.lead(memory, mem)
    return M.on() and memory ~= nil and mem ~= nil and (mem.ocean or 0) ~= 0
end

-- the ships go back to the engine (the fleet's again)
function M.handback(memory, mem)
    local st = memory ~= nil and memory.squadrons or nil
    if st == nil then
        return
    end
    for i = 1, SQ_MAX do
        local q = st.q ~= nil and st.q[i] or nil
        if q ~= nil and q.gid ~= nil then
            local g = G.get(memory, q.gid)
            if g ~= nil then
                G.disband(memory, g)
            end
            q.gid = nil
        end
    end
    memory.squadrons = nil
end

local function state(memory, now)
    local st = memory.squadrons
    if st == nil or (st.t or 0) > now then
        st = {q = {}, t = 0, look_t = 0, nlook = 0, zone_t = 0, look_at = {}, of = {}, split = {}, ms = {}, ms_t = 0,
              moved = 0, repairs = 0, flees = 0, drops = 0, behind = 0, shore_drops = 0, docked = 0, log_t = 0}
        for i = 1, SQ_MAX do
            st.q[i] = {i = i, on = false, u = {}, n = 0, want = 0, state = "rest", help = 0, call = false}
        end
        memory.squadrons = st   -- a new game (the time went back): the old groups are the engine's again
    end
    return st
end

-- the squadron's members: the lib's manual group holds the list (the lib drops the dead), ai.take holds them
local function set_members(memory, q, ids)
    if #ids == 0 then
        return
    end
    local g = q.gid ~= nil and G.get(memory, q.gid) or nil
    if g == nil then
        g = G.create(memory, {manual = true, task = "navy", role = "squad" .. q.i, x = q.zc_x or -1, y = q.zc_y or -1,
                              units = ids})
        q.gid = g.id
        return
    end
    ai.take(ids)
    g.units = ids   -- a manual group is the module's list (lib.groups' own words)
    if #ids > (g.started or 0) then
        g.started = #ids
    end
end

-- the counters by state (sq_state)
local STATE_COUNT = {engage = "engages", hold = "holds", help = "helps", push = "pushes", raid = "raids",
                     hunt = "hunts", flee = "flees", shell = "shells", strike = "strikes", scout = "scouted",
                     rest = "rests"}
local function sq_state(q, s, now)
    if q.state ~= s then
        q.state = s
        q.state_t = now
        local k = STATE_COUNT[s]
        if k ~= nil then
            q[k] = (q[k] or 0) + 1
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The water (fleet.lua's scan and helpers)

local function safe(fx, fy, tx, ty, oc)
    local wx, wy = F.safe_toward(fx, fy, tx, ty, oc, 3)
    if wx ~= nil then
        return wx, wy
    end
    wx, wy = F.water_near(tx, ty, oc)
    if wx ~= nil then
        return wx, wy
    end
    return tx, ty
end
-- the squadron's group move to (x, y): the game's move on its ships (fight: an attack-move)
local function sq_move(q, x, y, fight, now)
    if #q.u == 0 then
        return
    end
    ai.move(ids_of(q), x, y, {forced = not fight})
    q.ord_x, q.ord_y, q.order_t = x, y, now
end
-- sent again when its goal moved (5 tiles), or now and then while its ships stand idle away from it
local function sq_go(q, x, y, fight, now)
    q.go_x, q.go_y = x, y
    local far = 0
    for _, u in ipairs(q.u) do
        if u.action == "idle" and d2(u.x, u.y, x, y) > 64 then
            far = far + 1
        end
    end
    if q.ord_x == nil or d2(q.ord_x, q.ord_y, x, y) > 25 or (far > 0 and now - (q.order_t or 0) >= 20000) then
        sq_move(q, x, y, fight, now)
    end
end
-- the enemies of a sea: all of them known, or those within r of a point
local function foes_on(s, oc, x, y, r)
    local out = {}
    for _, e in ipairs(s.es) do
        if F.sea_of(e.x, e.y) == oc and (x == nil or d2(e.x, e.y, x, y) <= r * r) then
            out[#out + 1] = e
        end
    end
    return out
end
local function mean_of(list)
    local x, y, n = 0, 0, 0
    for _, e in ipairs(list) do
        x, y, n = x + e.x, y + e.y, n + 1
    end
    if n == 0 then
        return nil
    end
    return x // n, y // n
end
local function nearest_of(list, x, y)
    local best, bd = nil, nil
    for _, e in ipairs(list) do
        local d = d2(e.x, e.y, x, y)
        if bd == nil or d < bd then
            best, bd = e, d
        end
    end
    return best
end
-- an enemy frigate known within r of a point (the hunters' fear)
local function frigate_near(s, oc, x, y, r)
    for _, e in ipairs(s.es) do
        if F.sea_of(e.x, e.y) == oc and d2(e.x, e.y, x, y) <= r * r and F.role(e.type) == F.FRIG then
            return true
        end
    end
    return false
end
-- the haven of a zone: water by its own tower within the zone (farthest from the threat), else by a dock; out of danger
local function haven_of(q, tx, ty)
    local s = F.last_scan()
    local bx, by, bd = q.zc_x or -1, q.zc_y or -1, -1
    for pass = 1, 2 do
        local list = pass == 1 and s.towers or s.docks
        for _, b in ipairs(list) do
            if pass == 2 or d2(b.x, b.y, q.zc_x or 0, q.zc_y or 0) <= SQ_REACH * SQ_REACH then
                local wx, wy = F.water_near(b.x, b.y, q.ocean)
                if wx ~= nil and not F.in_danger(wx, wy) then
                    local d = tx ~= nil and d2(wx, wy, tx, ty) or 0
                    if d > bd then
                        bd, bx, by = d, wx, wy
                    end
                end
            end
        end
    end
    return bx, by
end
-- the nearest main squadron's middle on q's sea (else its haven)
local function to_main(st, q, cx, cy)
    local bx, by, bd = nil, nil, nil
    for j = 1, SQ_MAX do
        local m = st.q[j]
        if m.on and m.kind == MAIN and m.ocean == q.ocean and #m.u > 0 then
            local mx, my = centre_of(m)
            local d = d2(mx, my, cx, cy)
            if bd == nil or d < bd then
                bd, bx, by = d, mx, my
            end
        end
    end
    if bx == nil then
        return haven_of(q, cx, cy)
    end
    return bx, by
end
local function squadron_of(st, id)
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on then
            for _, u in ipairs(q.u) do
                if u.id == id then
                    return q
                end
            end
        end
    end
    return nil
end
-- out of a danger zone: away from its middle to its edge + 3 (or 45/90 degrees off), else to the squadron's point
local function flee_ship(mem, u, q)
    local z = F.in_danger(u.x, u.y)
    if z == nil then
        return false
    end
    local zone = (F.last_scan().dz or {})[z]
    if zone == nil then
        return false
    end
    local oc = F.sea_of(u.x, u.y)
    local gx = q ~= nil and q.go_x or mem.gx
    local gy = q ~= nil and q.go_y or mem.gy
    local dx, dy = u.x - zone.x, u.y - zone.y
    local len = isqrt(dx * dx + dy * dy)
    if len < 1 then
        dx, dy = (gx or u.x) - u.x, (gy or u.y) - u.y
        len = isqrt(dx * dx + dy * dy)
    end
    if len < 1 then
        dx, dy, len = 1, 0, 1
    end
    local go = math.max(zone.r - len + 3, 3)
    for _, rot in ipairs({{1000, 0}, {707, 707}, {707, -707}, {0, 1000}, {0, -1000}}) do
        local ex = (dx * rot[1] - dy * rot[2]) // (len * 1000)
        local ey = (dx * rot[2] + dy * rot[1]) // (len * 1000)
        local wx, wy = nil, nil
        if oc ~= 0 then
            wx, wy = F.water_near(u.x + ex * go, u.y + ey * go, oc)
        end
        if wx ~= nil and not F.in_danger(wx, wy) then
            if not (u.action == "move" and u.tx ~= nil and d2(u.tx, u.ty, wx, wy) <= 6) then
                ai.move(u.id, wx, wy, {forced = true})
            end
            return true
        end
    end
    if gx ~= nil then
        ai.move(u.id, gx, gy, {forced = true})
    end
    return true
end

-- ---------------------------------------------------------------------------------------------------------------
-- EACH SHIP (every fleet look): the danger zones, the targets in one, the repair, behind the squadron

-- the targets its ships attack now, by id (the scan keeps the seen ones only)
local function target_index(s)
    local out = {}
    for _, list in ipairs({s.es, s.ev, s.ec}) do
        for _, e in ipairs(list) do
            out[e.id] = e
        end
    end
    return out
end

local function care(view, memory, mem, st, s)
    local now = view.time
    local tgt = target_index(s)
    mem.docked = mem.docked or {}
    local docked = mem.docked
    -- repaired: ships back at 80% leave the docked list (fleet.c's own list, the repair's count)
    for _, id in ipairs(ai.keys(docked)) do
        local u = ai.unit(id)
        if u == nil or (u.hp or 0) * 10 >= 8 * (u.hp_max or 1) then
            docked[id] = nil
        end
    end
    st.docked = 0
    for _ in pairs(docked) do
        st.docked = st.docked + 1
    end
    -- the ships sent to the dock: to the water beside it again while they are farther than 4 tiles (fleet.c's fl_move)
    for _, id in ipairs(ai.keys(docked)) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil then
            local dk = F.repair_dock(u.x, u.y)
            if dk ~= nil and d2(dk.x, dk.y, u.x, u.y) > 16 then
                local oc = F.sea_of(u.x, u.y)
                local wx, wy = F.water_near(dk.x, dk.y, oc ~= 0 and oc or mem.ocean)
                if wx ~= nil and not (u.action == "move" and u.tx ~= nil and d2(u.tx, u.ty, wx, wy) <= 6) then
                    ai.move(u.id, wx, wy, {forced = true})
                end
            end
        end
    end
    for _, u in ipairs(s.ships) do
        if docked[u.id] == nil then
            local q = squadron_of(st, u.id)
            if flee_ship(mem, u, q) then
                st.flees = st.flees + 1
            else
                -- SEA: a damaged ship off to the dock before anything else, until 80%
                local hp_pct = u.hp_max > 0 and u.hp * 100 // u.hp_max or 100
                local dk = nil
                if S.fleet_cover ~= false and #s.docks > 0 and hp_pct < SQ_REPAIR then
                    local best, bd = nil, nil
                    for _, d in ipairs(s.docks) do
                        if not F.in_danger(d.x, d.y) then
                            local dd = d2(d.x, d.y, u.x, u.y)
                            if bd == nil or dd < bd then
                                best, bd = d, dd
                            end
                        end
                    end
                    dk = best
                end
                if dk ~= nil then
                    docked[u.id] = u.hp
                    st.repairs = st.repairs + 1
                    -- BEHIND THE SQUADRON: under fire, first behind the nearest main squadron within SQ_BEHIND_M
                    local placed = false
                    local ex, ey, ne = 0, 0, 0
                    for _, e in ipairs(s.es) do
                        if e.seen and d2(e.x, e.y, u.x, u.y) <= SQ_BEHIND_E * SQ_BEHIND_E then
                            ex, ey, ne = ex + e.x, ey + e.y, ne + 1
                        end
                    end
                    if ne > 0 then
                        ex, ey = ex // ne, ey // ne
                        local mc_x, mc_y, md = nil, nil, nil
                        for j = 1, SQ_MAX do
                            local q2 = st.q[j]
                            if q2.on and q2.kind == MAIN and q2.ocean == F.sea_of(u.x, u.y) and #q2.u >= 2 then
                                local mx, my = centre_of(q2)
                                local dd = d2(mx, my, u.x, u.y)
                                if dd <= SQ_BEHIND_M * SQ_BEHIND_M and (md == nil or dd < md) then
                                    md, mc_x, mc_y = dd, mx, my
                                end
                            end
                        end
                        if mc_x ~= nil then
                            local dx, dy = mc_x - ex, mc_y - ey
                            local len = isqrt(dx * dx + dy * dy)
                            if len > 0 then
                                local bx = mc_x + dx * SQ_BEHIND // len
                                local by = mc_y + dy * SQ_BEHIND // len
                                local wx, wy = F.water_near(bx, by, F.sea_of(u.x, u.y))
                                if wx ~= nil and not F.in_danger(wx, wy)
                                        and not (u.action == "move" and u.tx ~= nil and d2(u.tx, u.ty, wx, wy) <= 6) then
                                    ai.move(u.id, wx, wy, {forced = true})
                                    st.behind = st.behind + 1
                                    placed = true
                                end
                            end
                        end
                    end
                    if not placed and d2(dk.x, dk.y, u.x, u.y) > 16 then
                        local oc = F.sea_of(u.x, u.y)
                        local wx, wy = F.water_near(dk.x, dk.y, oc ~= 0 and oc or mem.ocean)
                        if wx ~= nil then
                            ai.move(u.id, wx, wy, {forced = true})
                        end
                    end
                elseif u.action == "attack" and u.target ~= nil then
                    -- a target inside a danger zone dropped; a land target only within its reach (THE SHORE)
                    local e = tgt[u.target]
                    if e ~= nil then
                        local drop = F.in_danger(e.x, e.y, 1) ~= nil
                        if not drop and S.sea_shore_fire ~= false and e.category ~= "ship" and e.category ~= "boat"
                                and e.category ~= "transport" then
                            local range = ((F.cls(u.type).range or 0) // 100) + 1
                            drop = isqrt(d2(u.x, u.y, e.x, e.y)) > range
                        end
                        if drop then
                            local bx = q ~= nil and q.go_x or mem.gx
                            local by = q ~= nil and q.go_y or mem.gy
                            if bx ~= nil and not F.in_danger(bx, by) then
                                if not (u.action == "move" and u.tx ~= nil and d2(u.tx, u.ty, bx, by) <= 6) then
                                    ai.move(u.id, bx, by, {forced = true})
                                end
                                st.drops = st.drops + 1
                                if S.sea_shore_fire ~= false then
                                    st.shore_drops = st.shore_drops + 1
                                end
                            end
                        end
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The zones and the squadrons (SQ_ZONES)

-- a water territory's worth to it (sq_sea_value, without ferry.c's ways: no ferry in the port): its coast along land it
-- holds and the fish it has seen there / 200, doubled where enemy warships were seen lately (+20)
local function sea_value(wi, now)
    local recent = wi.border or (wi.eship_t > 0 and now >= wi.eship_t and now - wi.eship_t < 300000)
    local v = (wi.coast or 0) * 100 + (wi.fish or 0) // 2
    if v <= 0 and not recent then
        return 0
    end
    return v * (recent and 2 or 1) + (recent and 2000 or 0)
end

-- the seat of a squadron: the one of the same sea and kind nearest the anchor (its ships stay with it, the zone moves),
-- else a free slot; nil when the eight are taken
local function seat(st, mark, kind, oc, anchor, zc_x, zc_y, zvalue, want)
    local found, bd = nil, nil
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on and q.kind == kind and q.ocean == oc and q.seated ~= mark then
            local cx, cy = centre_of(q)
            local d = anchor ~= nil and d2(cx, cy, zc_x, zc_y) or 0
            if bd == nil or d < bd then
                bd, found = d, q
            end
        end
    end
    if found == nil then
        for i = 1, SQ_MAX do
            local q = st.q[i]
            if not q.on and q.seated ~= mark then
                found = q
                break
            end
        end
    end
    if found == nil then
        return nil
    end
    if not found.on then
        found.on = true
        found.u = {}
        found.n = 0
        found.state, found.help, found.call, found.target_r, found.shell = "rest", 0, false, nil, nil
    end
    found.kind = kind
    found.ocean = oc
    found.anchor = anchor
    found.zc_x, found.zc_y = zc_x, zc_y
    found.zvalue = zvalue or 0
    found.want = math.max(want or found.want or 0, 1)
    found.seated = mark
    return found
end

local function zones(st, look, now)
    st.zone_t = now
    for _, sea in ipairs(look.seas) do
        local oc = sea.oc
        -- the water territories of worth on it, the most valuable first
        local list = {}
        for _, w in ipairs(sea.waters) do
            local v = sea_value(w, now)
            if v > 0 then
                list[#list + 1] = {r = w.r, x = w.x, y = w.y, v = v}
            end
        end
        table.sort(list, function(a, b)
            if a.v ~= b.v then
                return a.v > b.v
            end
            return a.r < b.r
        end)
        -- the anchors: every water within SQ_REACH of the most valuable uncovered one is that zone's
        local anchors, covered = {}, {}
        for a = 1, #list do
            if not covered[a] and #anchors < SQ_MAX then
                local an = {r = list[a].r, x = list[a].x, y = list[a].y, v = 0}
                for b = a, #list do
                    if not covered[b] and d2(list[b].x, list[b].y, list[a].x, list[a].y) <= SQ_REACH * SQ_REACH then
                        covered[b] = true
                        an.v = an.v + list[b].v
                    end
                end
                anchors[#anchors + 1] = an
            end
        end
        local split = st.split[oc] == true
        local na = #anchors
        if na == 0 then
            anchors[1] = {r = nil, x = sea.cx or 0, y = sea.cy or 0, v = 1}   -- no water of worth yet: one by its ships
            na = 1
        elseif not split then
            na = 1   -- CONCENTRATION: not split: one main squadron on the sea
        end
        -- the groups by the ships
        local scouts = sea.nfrig >= 2 and (na > 1 and 2 or 1) or 0
        local hunters = math.min((sea.nsub + SQ_HUNT_N - 1) // SQ_HUNT_N, 3)
        local escorts = math.min(scouts * SQ_SCOUT_N, sea.nfrig)
        local mains = (sea.n - sea.nsub - sea.nmsub - escorts) // SQ_MAIN_MIN
        if mains < 1 then
            mains = 1
        end
        -- STEADY: as many as before unless the ships make a clear difference (one squadron's minimum and 2)
        local had = 0
        for i = 1, SQ_MAX do
            local q = st.q[i]
            if q.on and q.kind == MAIN and q.ocean == oc then
                had = had + 1
            end
        end
        if had > 0 and mains ~= had and math.abs((sea.n - sea.nsub - sea.nmsub) - had * SQ_MAIN_MIN) < SQ_MAIN_MIN + 2 then
            mains = had
        end
        if mains > na then
            mains = na
        end
        for k = 1, mains do
            local an = anchors[k]
            seat(st, mark, MAIN, oc, an.r, an.x, an.y, an.v, SQ_MAIN_MIN)
        end
        for _ = 1, scouts do
            seat(st, mark, SCOUT, oc, nil, sea.cx or 0, sea.cy or 0, 0, SQ_SCOUT_N)
        end
        for _ = 1, hunters do
            seat(st, mark, HUNT, oc, nil, sea.cx or 0, sea.cy or 0, 0, math.max((sea.nsub + hunters - 1) // hunters, 1))
        end
        local strikes = math.min((sea.nmsub + 1) // 2, 2)
        for _ = 1, strikes do
            seat(st, mark, STRIKE, oc, nil, sea.cx or 0, sea.cy or 0, 0,
                 (sea.nmsub + strikes - 1) // strikes + SQ_STRIKE_FRIG + 1)
        end
    end
    -- its zone gone: its ships go to the others
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on and q.seated ~= mark then
            q.on = false
            q.u = {}
            q.n = 0
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The ships and their squadrons (sq_assign)

-- the strike group short of a frigate or a cruiser (its escort), or nil
local function strike_short(st, esc, oc, role)
    for i = 1, SQ_MAX do
        local q = st.q[i]
        local e = esc[i]
        if q.on and q.ocean == oc and e ~= nil then
            if role == F.FRIG and e.f > 0 then
                return i
            elseif role == F.AA and e.a > 0 then
                return i
            end
        end
    end
    return nil
end

local function assign(memory, st, look, now)
    for i = 1, SQ_MAX do
        st.q[i].u = {}
        st.q[i].n = 0
    end
    -- the main squadrons' shares on each sea by their zones' worth
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on and q.kind == MAIN then
            local sum, ships, scouts, sea_n = 0, 0, 0, nil
            for j = 1, SQ_MAX do
                local o = st.q[j]
                if o.on and o.ocean == q.ocean then
                    if o.kind == MAIN then
                        sum = sum + (o.zvalue or 0)
                    elseif o.kind == SCOUT then
                        scouts = scouts + SQ_SCOUT_N
                    end
                end
            end
            for _, sea in ipairs(look.seas) do
                if sea.oc == q.ocean then
                    sea_n = sea
                end
            end
            if sea_n ~= nil then
                ships = sea_n.n - sea_n.nsub - sea_n.nmsub - scouts
                if sum > 0 and ships > 0 then
                    q.want = (ships * (q.zvalue or 0) + sum // 2) // sum
                else
                    q.want = math.max(ships, SQ_MAIN_MIN)
                end
                if q.want < SQ_MAIN_MIN then
                    q.want = SQ_MAIN_MIN
                end
            end
        end
    end
    -- THE STRIKE's escort it lacks, from the last look's members
    local esc = {}
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on and q.kind == STRIKE then
            esc[i] = {f = SQ_STRIKE_FRIG, a = 1}
            for _, u in ipairs(q.u) do
                if u.role == F.FRIG then
                    esc[i].f = esc[i].f - 1
                elseif u.role == F.AA then
                    esc[i].a = esc[i].a - 1
                end
            end
        end
    end
    local prev = st.of
    local kept, of = {}, {}
    for pass = 0, 1 do
        for _, u in ipairs(look.ships) do
            if kept[u.id] == nil then
                local oc, role, q = u.oc, u.role, nil
                if pass == 0 then
                    -- its squadron still, while that one has room for it
                    local was = prev[u.id]
                    if was ~= nil and was >= 1 and was <= SQ_MAX and st.q[was].on and st.q[was].ocean == oc
                            and fits(st.q[was].kind, role) and #st.q[was].u < st.q[was].want + 2 then
                        q = st.q[was]
                        -- THE STRIKE's escort first: its frigate or cruiser goes while a group is short
                        if q.kind == MAIN then
                            local s2 = strike_short(st, esc, oc, role)
                            if s2 ~= nil then
                                esc[s2][role == F.FRIG and "f" or "a"] = esc[s2][role == F.FRIG and "f" or "a"] - 1
                                q = nil
                            end
                        end
                    end
                else
                    -- the squadron the most short of it
                    local best = nil
                    for i = 1, SQ_MAX do
                        local o = st.q[i]
                        if o.on and o.ocean == oc and fits(o.kind, role) then
                            local short = o.want - #o.u + (o.kind ~= MAIN and #o.u < o.want and 100 or 0)
                            if role == ROLE_M and o.kind == STRIKE and #o.u < o.want then
                                short = short + 1000   -- a strike group's subs first
                            end
                            if best == nil or short > best then
                                best, q = short, o
                            end
                        end
                    end
                end
                if q ~= nil then
                    kept[u.id] = true
                    q.u[#q.u + 1] = u
                    q.n = #q.u
                    if prev[u.id] ~= nil and prev[u.id] ~= q.i then
                        st.moved = (st.moved or 0) + 1
                    end
                    of[u.id] = q.i
                end
            end
        end
    end
    st.of = of
    -- the groups hold the members (the lib drops the dead)
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on and #q.u > 0 then
            set_members(memory, q, ids_of(q))
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The squadrons' decisions

local shell_step   -- THE SHORE, below (main_step asks it)

-- a main squadron's targets: the enemy warships in its zone, or near a dock or fishing boat of its on this sea (HOME)
local function main_threat(st, q, s)
    local home_x, home_y
    if q.anchor ~= nil then
        home_x, home_y = q.zc_x, q.zc_y
    else
        home_x, home_y = centre_of(q)
    end
    local foes = foes_on(s, q.ocean, home_x, home_y, SQ_REACH)
    if #foes > 0 then
        return foes, home_x, home_y
    end
    local cx, cy = centre_of(q)
    local cands = {}
    for _, dk in ipairs(s.docks) do
        cands[#cands + 1] = {x = dk.x, y = dk.y}
    end
    for _, id in ipairs(s.boats) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil then
            cands[#cands + 1] = {x = u.x, y = u.y}
        end
    end
    local bp, bd = nil, nil
    for _, b in ipairs(cands) do
        local d = d2(b.x, b.y, cx, cy)
        if (bd == nil or d < bd) and (F.sea_of(b.x, b.y) == q.ocean or F.water_near(b.x, b.y, q.ocean) ~= nil) then
            local f2 = foes_on(s, q.ocean, b.x, b.y, SQ_HOME_R)
            if #f2 > 0 then
                -- only the nearest main squadron answers a place
                local nearer = false
                for j = 1, SQ_MAX do
                    local o = st.q[j]
                    if j ~= q.i and o.on and o.kind == MAIN and o.ocean == q.ocean and #o.u > 0 then
                        local ox, oy = centre_of(o)
                        if d2(ox, oy, b.x, b.y) < d then
                            nearer = true
                        end
                    end
                end
                if not nearer then
                    bd, bp = d, {x = b.x, y = b.y, foes = f2}
                end
            end
        end
    end
    if bp ~= nil then
        return bp.foes, bp.x, bp.y
    end
    return {}, home_x, home_y
end

local function main_step(st, q, s, now, whole)
    local cx, cy = centre_of(q)
    q.call = false
    local foes, hx, hy = main_threat(st, q, s)
    if #foes > 0 then
        local r = ratio_of(q, foes)
        q.ratio = r
        local ex, ey = mean_of(foes)
        if q.state == "help" or r >= SQ_ENGAGE or (q.state == "engage" and r >= SQ_HOLD) then
            sq_state(q, "engage", now)
            local wx, wy = safe(cx, cy, ex, ey, q.ocean)
            sq_go(q, wx, wy, true, now)
        else
            -- it holds under its own towers (or by its dock) and calls for help
            sq_state(q, "hold", now)
            q.call = true
            local bx, by = haven_of(q, ex, ey)
            sq_go(q, bx, by, false, now)
        end
        return
    end
    -- a call from another squadron of the same sea answered while its own zone is quiet and it has half its hit points
    if q.state == "help" and (q.help == 0 or not st.q[q.help].on or not st.q[q.help].call) then
        q.help = 0
    end
    if q.help == 0 and hp_share(q) >= 50 then
        for j = 1, SQ_MAX do
            local o = st.q[j]
            if j ~= q.i and o.on and o.kind == MAIN and o.ocean == q.ocean and o.call and #o.u > 0 then
                q.help = j
                break
            end
        end
    end
    if q.help ~= 0 then
        local h = st.q[q.help]
        sq_state(q, "help", now)
        local mx, my = centre_of(h)
        local wx, wy = safe(cx, cy, mx, my, q.ocean)
        sq_go(q, wx, wy, true, now)
        return
    end
    -- A PUSH: clearly the stronger against those within 2 x SQ_REACH of its zone, not weaker than all it knows on the
    -- sea (CONCENTRATION)
    local far = foes_on(s, q.ocean, hx, hy, 2 * SQ_REACH)
    if #far > 0 and #q.u >= SQ_MAIN_MIN and hp_share(q) >= 60 and whole >= 100 then
        local r = ratio_of(q, far)
        if r >= SQ_PUSH or (q.state == "push" and r >= 110) then
            local e = nearest_of(far, cx, cy)
            q.ratio = r
            sq_state(q, "push", now)
            local wx, wy = safe(cx, cy, e.x, e.y, q.ocean)
            sq_go(q, wx, wy, true, now)
            return
        end
    end
    -- THE SHORE: SHELLING
    if S.sea_shore_fire ~= false and shell_step(q, s, now, hx, hy) then
        return
    end
    q.shell = nil
    sq_state(q, "rest", now)
    if q.anchor ~= nil then
        local wx, wy = safe(cx, cy, q.zc_x, q.zc_y, q.ocean)
        sq_go(q, wx, wy, S.sea_shore_fire == false, now)   -- THE SHORE: a plain move
    end
end

-- THE SHORE: the defences that reach the water off (bx, by) - the enemy towers and the land units whose weapon threatens
-- ships, seen within their reach + SQ_SHELL_DEF - and the one nearest the squadron
local function shore_defs(bx, by, cx, cy)
    local ids = ai.seen{enemy = true, near = {bx, by, 24}, limit = 16}
    local out = {}
    if #ids == 0 then
        return out, nil
    end
    local cols = ai.get(ids, {"x", "y", "type", "category", "hp"})
    local closest, cd = nil, nil
    for k = 1, #ids do
        if cols.x[k] ~= nil and (cols.hp[k] or 0) > 0 then
            local rec = {type = cols.type[k], building = cols.category[k] == "building", category = cols.category[k]}
            local r = F.danger_r(rec) + SQ_SHELL_DEF
            if r > SQ_SHELL_DEF and d2(cols.x[k], cols.y[k], bx, by) <= r * r then
                out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], hp = cols.hp[k], type = cols.type[k],
                                 category = cols.category[k]}
                local d = d2(cols.x[k], cols.y[k], cx, cy)
                if cd == nil or d < cd then
                    cd, closest = d, out[#out]
                end
            end
        end
    end
    return out, closest
end

-- a main squadron at rest shells a building on its sea's coast when its estimate beats the defences that reach the
-- water there: 1 while it shells, 0 not
function shell_step(q, s, now, hx, hy)
    local hp = hp_share(q)
    local t = nil
    if q.shell ~= nil then
        local u = ai.unit(q.shell.id)
        if u ~= nil and (u.hp or 0) > 0 and u.category == "building" then
            t = {id = q.shell.id, x = u.x, y = u.y, type = u.type}
        else
            q.shell = nil
        end
    end
    local cx, cy = centre_of(q)
    if t ~= nil then
        local defs = shore_defs(t.x, t.y, cx, cy)
        local r = ratio_of(q, defs)
        if r < SQ_SHELL_HOLD or hp < SQ_SHELL_OFF then
            t = nil
            q.shell = nil
        else
            q.ratio = r
        end
    end
    if t == nil and #q.u >= 3 and hp >= SQ_SHELL_HP then
        -- the nearest buildings it sees on its sea's coast near its zone, while it beats what reaches them
        local cands = {}
        for _, e in ipairs(s.ec) do
            if e.category == "building" and #cands < SQ_SHELL_LOOKS
                    and d2(e.x, e.y, hx, hy) <= SQ_SHELL_R * SQ_SHELL_R
                    and F.water_near(e.x, e.y, q.ocean) ~= nil then
                cands[#cands + 1] = e
            end
        end
        table.sort(cands, function(a, b)
            local da, db = d2(a.x, a.y, hx, hy), d2(b.x, b.y, hx, hy)
            if da ~= db then
                return da < db
            end
            return a.id < b.id
        end)
        for k = 1, math.min(#cands, SQ_SHELL_LOOKS) do
            local e = cands[k]
            local defs = shore_defs(e.x, e.y, cx, cy)
            local r = ratio_of(q, defs)
            if r >= SQ_SHELL_GO then
                t = e
                q.ratio = r
                q.shell = {id = e.id}
                print(string.format("squadrons main %d shells building %s at %d,%d - %d defences reach it, estimate %.2f",
                    q.i, tostring(e.type), e.x, e.y, #defs, r / 100.0))
                break
            end
        end
    end
    if t == nil then
        return false
    end
    -- the defences reaching it first, the nearest
    local defs, closest = shore_defs(t.x, t.y, cx, cy)
    local aim = closest ~= nil and closest or t
    sq_state(q, "shell", now)
    if q.order_t == nil or now - q.order_t >= 3000 then
        ai.attack(ids_of(q), aim.id)
        q.order_t = now
        q.ord_x, q.ord_y = aim.x, aim.y
    end
    q.go_x, q.go_y = aim.x, aim.y
    return true
end

-- HUNT: the submarines and galleys after the enemy warships no frigate is near
local function hunt_step(st, q, s, now)
    local cx, cy = centre_of(q)
    if frigate_near(s, q.ocean, cx, cy, SQ_FRIG_R) then
        sq_state(q, "flee", now)
        local bx, by = to_main(st, q, cx, cy)
        local wx, wy = safe(cx, cy, bx, by, q.ocean)
        sq_go(q, wx, wy, false, now)
        return
    end
    local best, bd = nil, nil
    for _, e in ipairs(s.es) do
        if F.sea_of(e.x, e.y) == q.ocean and F.role(e.type) ~= F.FRIG and not F.in_danger(e.x, e.y) then
            local d = d2(e.x, e.y, cx, cy)
            if d <= SQ_HUNT_R * SQ_HUNT_R and (bd == nil or d < bd)
                    and not frigate_near(s, q.ocean, e.x, e.y, SQ_FRIG_R) then
                local comp = foes_on(s, q.ocean, e.x, e.y, 8)
                local r = ratio_of(q, comp)
                if r >= 100 then
                    bd, best, q.ratio = d, e, r
                end
            end
        end
    end
    if best ~= nil then
        sq_state(q, "hunt", now)
        if now - (q.order_t or 0) >= 3000 then
            ai.attack(ids_of(q), best.id)
            q.order_t = now
        end
        q.ord_x, q.ord_y = best.x, best.y
        q.go_x, q.go_y = best.x, best.y
        return
    end
    sq_state(q, "rest", now)
    local bx, by = to_main(st, q, cx, cy)
    local wx, wy = safe(cx, cy, bx, by, q.ocean)
    sq_go(q, wx, wy, S.sea_shore_fire == false, now)
end

-- SCOUT: the enemy's fishing boats and transports raided, and the waters not seen longest
local function scout_step(st, q, s, now, look)
    local cx, cy = centre_of(q)
    local near = foes_on(s, q.ocean, cx, cy, SQ_SCOUT_FLEE)
    if #near > 0 then
        local r = ratio_of(q, near)
        q.ratio = r
        if r < SQ_SCOUT_FIGHT then
            sq_state(q, "flee", now)
            local bx, by = to_main(st, q, cx, cy)
            local wx, wy = safe(cx, cy, bx, by, q.ocean)
            sq_go(q, wx, wy, false, now)
            return
        end
    end
    -- A RAID: prey within SQ_RAID_R where no warship near it stands a fight with the group
    local best, bd = nil, nil
    for _, e in ipairs(s.ev) do
        if F.sea_of(e.x, e.y) == q.ocean and not F.in_danger(e.x, e.y) then
            local d = d2(e.x, e.y, cx, cy)
            if d <= SQ_RAID_R * SQ_RAID_R and (bd == nil or d < bd) then
                local war = foes_on(s, q.ocean, e.x, e.y, 12)
                if #war == 0 or ratio_of(q, war) >= SQ_SCOUT_FIGHT then
                    bd, best = d, e
                end
            end
        end
    end
    if best ~= nil then
        sq_state(q, "raid", now)
        if now - (q.order_t or 0) >= 3000 then
            ai.attack(ids_of(q), best.id)
            q.order_t = now
        end
        q.ord_x, q.ord_y = best.x, best.y
        q.go_x, q.go_y = best.x, best.y
        return
    end
    -- SCOUTING: the waters of its sea not seen longest (the unheld first); a new one when there, or after 90 s
    local there = q.target_r ~= nil and (look.look_at[q.target_r] or 0) >= (q.target_t or 0)
    if q.target_r ~= nil and (there or now - (q.target_t or 0) >= 90000) then
        q.scouted = (q.scouted or 0) + (there and 1 or 0)
        q.target_r = nil
    end
    if q.target_r == nil then
        local bp, bw = nil, 0
        for _, w in ipairs(look.waters) do
            if w.oc == q.ocean and not F.in_danger(w.x, w.y) then
                local seen = look.look_at[w.r]
                local age = seen == nil and 10000000 or (now - seen)
                if seen == nil or age >= 300000 then
                    local taken = false
                    for j = 1, SQ_MAX do
                        local o = st.q[j]
                        if o.on and o.kind == SCOUT and o.target_r == w.r then
                            taken = true
                        end
                    end
                    if not taken then
                        local wv = age * (w.role ~= 0 and 1 or 2) // (1 + isqrt(d2(w.x, w.y, cx, cy)) // 20)
                        if wv > bw then
                            bw, bp = wv, w
                        end
                    end
                end
            end
        end
        if bp ~= nil then
            q.target_r = bp.r
        end
        q.target_t = now
    end
    if q.target_r ~= nil then
        local w = nil
        for _, ww in ipairs(look.waters) do
            if ww.r == q.target_r then
                w = ww
            end
        end
        if w ~= nil then
            sq_state(q, "scout", now)
            local wx, wy = safe(cx, cy, w.x, w.y, q.ocean)
            sq_go(q, wx, wy, S.sea_shore_fire == false, now)
            return
        end
        q.target_r = nil
    end
    -- nothing to look at: with the nearest main squadron
    for j = 1, SQ_MAX do
        local o = st.q[j]
        if o.on and o.kind == MAIN and o.ocean == q.ocean and #o.u > 0 then
            sq_state(q, "rest", now)
            local mx, my = centre_of(o)
            local wx, wy = safe(cx, cy, mx, my, q.ocean)
            sq_go(q, wx, wy, S.sea_shore_fire == false, now)
            return
        end
    end
end

-- THE STRIKE: the missile submarines out where the enemy's troops and buildings stand near the water (a tower or a
-- siege weapon x3, a building x0.5, a soldier its hit points; a wall is no target)
local function is_wall(t)
    local p = string.sub(t or "", 1, 7)
    return p == "b_wall_" or p == "b_gate_"
end
local function is_tower(t)
    local p = string.sub(t or "", 1, 8)
    return p == "b_tower_" or string.sub(t or "", 1, 14) == "b_guard_tower_"
end
local function strike_worth(e)
    local w = e.hp or 0
    if e.category == "building" then
        w = is_tower(e.type) and w * 3 or w // 2
    elseif e.category == "siege" then
        w = w * 3
    end
    return w
end
local function strike_step(st, q, s, now)
    local cx, cy = centre_of(q)
    local subs = 0
    for _, u in ipairs(q.u) do
        if u.role == ROLE_M then
            subs = subs + 1
        end
    end
    local near = foes_on(s, q.ocean, cx, cy, SQ_STRIKE_FLEE)
    if #near > 0 and ratio_of(q, near) < 100 then
        sq_state(q, "flee", now)
        local bx, by = to_main(st, q, cx, cy)
        local wx, wy = safe(cx, cy, bx, by, q.ocean)
        sq_go(q, wx, wy, false, now)
        return
    end
    if subs == 0 then
        sq_state(q, "rest", now)
        local bx, by = to_main(st, q, cx, cy)
        local wx, wy = safe(cx, cy, bx, by, q.ocean)
        sq_go(q, wx, wy, false, now)
        return
    end
    -- the enemy on land it sees now (its other ships' sight), each worth its hit points
    local ids = ai.seen{enemy = true, near = {cx, cy, SQ_STRIKE_R + SQ_STRIKE_RANGE}, limit = SQ_STRIKE_N}
    local list = {}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "type", "category", "hp"})
        for k = 1, #ids do
            local cat = cols.category[k]
            if cols.x[k] ~= nil and (cols.hp[k] or 0) > 0
                    and (cat == "building" or cat == "soldier" or cat == "siege")
                    and not is_wall(cols.type[k]) and F.sea_of(cols.x[k], cols.y[k]) == 0 then
                local e = {id = ids[k], x = cols.x[k], y = cols.y[k], category = cat, type = cols.type[k], hp = cols.hp[k]}
                e.worth = strike_worth(e)
                list[#list + 1] = e
            end
        end
    end
    local bp, bw, bx, by = nil, SQ_STRIKE_MIN, nil, nil
    for _, e in ipairs(list) do
        local w = 0
        for _, o in ipairs(list) do
            if d2(o.x, o.y, e.x, e.y) <= SQ_STRIKE_AREA * SQ_STRIKE_AREA then
                w = w + o.worth
            end
        end
        if w > bw then
            local wx, wy = F.water_near(e.x, e.y, q.ocean)
            if wx ~= nil then
                local fx, fy = safe(cx, cy, wx, wy, q.ocean)
                if not F.in_danger(fx, fy) and d2(fx, fy, e.x, e.y) <= SQ_STRIKE_RANGE * SQ_STRIKE_RANGE
                        and d2(fx, fy, cx, cy) <= SQ_STRIKE_R * SQ_STRIKE_R then
                    bw, bp, bx, by = w, e, fx, fy
                end
            end
        end
    end
    if bp == nil then
        sq_state(q, "rest", now)
        local mx, my = to_main(st, q, cx, cy)
        local wx, wy = safe(cx, cy, mx, my, q.ocean)
        sq_go(q, wx, wy, false, now)
        return
    end
    sq_state(q, "strike", now)
    -- each missile sub in reach: the most worth it reaches; the escort stays with them
    local there, ordered = 0, 0
    for _, u in ipairs(q.u) do
        if u.role == ROLE_M then
            local reach = ((F.cls(u.type).range or 0) // 100) - 1
            local bj, bv = nil, 0
            for _, e in ipairs(list) do
                if e.worth > bv and d2(e.x, e.y, u.x, u.y) <= reach * reach then
                    bj, bv = e, e.worth
                end
            end
            if bj ~= nil then
                there = there + 1
                if u.target ~= bj.id and now - (q.order_t or 0) >= 3000 then
                    ai.attack(u.id, bj.id)
                    ordered = ordered + 1
                end
            end
        end
    end
    if ordered > 0 then
        q.order_t = now
    end
    if there == 0 then
        sq_go(q, bx, by, false, now)
    end
    q.go_x, q.go_y = bx, by
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step

-- its warships afloat (the fleet's scan leaves the missile subs out: no role); the subs looked for every 10 s
local function ships_of(st, s, now)
    local out = {}
    for _, r in ipairs(s.ships) do
        out[#out + 1] = {id = r.id, x = r.x, y = r.y, hp = r.hp, hp_max = r.hp_max, type = r.type,
                         role = F.role(r.type), action = r.action, target = r.target, tx = r.tx, ty = r.ty,
                         oc = F.sea_of(r.x, r.y)}
    end
    if st.ms_t == nil or now < st.ms_t or now - st.ms_t >= SQ_MSUB_LOOK then
        st.ms_t = now
        st.ms = {}
        local seen = {}
        for _, r in ipairs(out) do
            seen[r.id] = true
        end
        for _, id in ipairs(ai.units{category = "ship", armed = true}) do
            if not seen[id] then
                local u = ai.unit(id)
                if u ~= nil and u.x ~= nil and is_msub(u.type) then
                    st.ms[#st.ms + 1] = {id = id, x = u.x, y = u.y, hp = u.hp or 0, hp_max = u.hp_max or 1,
                                         type = u.type, role = ROLE_M, action = u.action, target = u.target,
                                         tx = u.tx, ty = u.ty, oc = F.sea_of(u.x, u.y)}
                end
            end
        end
    end
    for _, r in ipairs(st.ms) do
        local u = ai.unit(r.id)
        if u ~= nil and u.x ~= nil then
            r.x, r.y, r.hp, r.hp_max, r.action, r.target = u.x, u.y, u.hp or 0, u.hp_max or 1, u.action, u.target
            r.oc = F.sea_of(u.x, u.y)
            out[#out + 1] = r
        end
    end
    return out
end

-- the seas: its ships by the engine's sea, the whole fight estimate (CONCENTRATION), the waters of worth
local function seas_of(st, s, ships, waters)
    local seas, byoc = {}, {}
    for _, u in ipairs(ships) do
        if u.oc ~= 0 then
            local sea = byoc[u.oc]
            if sea == nil then
                sea = {oc = u.oc, n = 0, nfrig = 0, nsub = 0, nmsub = 0, sx = 0, sy = 0, waters = {}, u = {}}
                byoc[u.oc] = sea
                seas[#seas + 1] = sea
            end
            sea.n = sea.n + 1
            sea.sx, sea.sy = sea.sx + u.x, sea.sy + u.y
            sea.u[#sea.u + 1] = u
            if u.role == F.FRIG then
                sea.nfrig = sea.nfrig + 1
            elseif hunter(u.role) then
                sea.nsub = sea.nsub + 1
            elseif u.role == ROLE_M then
                sea.nmsub = sea.nmsub + 1
            end
        end
    end
    for _, sea in ipairs(seas) do
        sea.cx, sea.cy = sea.sx // sea.n, sea.sy // sea.n
        for _, w in ipairs(waters) do
            if w.oc == sea.oc then
                sea.waters[#sea.waters + 1] = w
            end
        end
        -- CONCENTRATION: the sea's whole ratio from SQ_SPLIT, together under 1.0
        local foes = foes_on(s, sea.oc)
        local r = #foes > 0 and ratio_of(sea, foes) or 9900
        sea.ratio = r
        if not st.split[sea.oc] and r >= SQ_SPLIT then
            st.split[sea.oc] = true
            st.zone_t = 0
        elseif st.split[sea.oc] and r < 100 then
            st.split[sea.oc] = false
            st.zone_t = 0
        end
        sea.split = st.split[sea.oc] == true
    end
    return seas
end

-- the waters of worth (its sea territories), a look every 10 s (the seas' coasts and threats move slowly)
local function waters_of(st, now)
    if st.waters ~= nil and st.waters_t ~= nil and now >= st.waters_t and now - st.waters_t < 10000 then
        return st.waters
    end
    st.waters_t = now
    local out = {}
    for r = 1, T.count() do
        local wi = T.info(r)
        if wi ~= nil and not wi.land then
            out[#out + 1] = {r = r, x = wi.x, y = wi.y, oc = wi.ocean, role = wi.role, coast = wi.coast,
                             fish = wi.fish or 0, border = wi.border, eship_t = wi.eship_t}
        end
    end
    st.waters = out
    return out
end

-- the minute line
local function sq_log(st, seas)
    local nsq, ships = 0, 0
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on then
            nsq = nsq + 1
            ships = ships + #q.u
        end
    end
    local seas_s = ""
    for _, sea in ipairs(seas) do
        seas_s = seas_s .. string.format(" | sea %d: %d ships vs %d known %s, ratio %.2f", sea.oc, sea.n,
            #foes_on(F.last_scan(), sea.oc), sea.split and "split" or "one", (sea.ratio or 0) / 100.0)
    end
    print(string.format("squadrons %d groups, %d ships, moved %d, repairs %d, docked %d, flees %d, drops %d (%d on land), behind %d%s",
        nsq, ships, st.moved or 0, st.repairs or 0, st.docked or 0, st.flees or 0, st.drops or 0, st.shore_drops or 0,
        st.behind or 0, seas_s))
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on then
            local cx, cy = centre_of(q)
            print(string.format("squadrons %d %s sea %d%s at %d,%d: %d/%d ships at %d,%d %s ratio %.2f fought %d held %d helped %d pushed %d raided %d ran %d shelled %d",
                i, q.kind, q.ocean, q.anchor ~= nil and (" zone " .. q.anchor) or "", q.zc_x or -1, q.zc_y or -1, #q.u,
                q.want, cx, cy, q.state, (q.ratio or 0) / 100.0, q.engages or 0, q.holds or 0, q.helps or 0,
                q.pushes or 0, q.raids or 0, q.flees or 0, q.shells or 0))
        end
    end
end

function M.step(view, memory, mem, s, exclude)
    local now = view.time
    local st = state(memory, now)
    st.t = now
    -- A LOOK AGAIN: the waters its warships stand in (the scouting's memory)
    for _, u in ipairs(s.ships) do
        local r = T.terr_at(u.x, u.y)
        if r ~= nil then
            st.look_at[r] = now > 0 and now or 1
        end
    end
    -- EACH SHIP, every look of the fleet's
    care(view, memory, mem, st, s)
    if st.look_t ~= nil and now >= st.look_t and now - st.look_t < SQ_LOOK then
        return
    end
    st.look_t = now
    st.nlook = (st.nlook or 0) + 1
    local ships = ships_of(st, s, now)
    if #ships == 0 or T.count() == 0 then
        return
    end
    local waters = waters_of(st, now)
    local seas = seas_of(st, s, ships, waters)
    local look = {seas = seas, waters = waters, look_at = st.look_at, ships = ships}
    -- the zones every SQ_ZONES (or when a sea's concentration changed)
    if st.zone_t == 0 or (now >= st.zone_t and now - st.zone_t >= SQ_ZONES) then
        zones(st, look, now)
    else
        for i = 1, SQ_MAX do
            st.q[i].seated = st.nlook
        end
    end
    assign(memory, st, look, now)
    -- each squadron's decision
    for i = 1, SQ_MAX do
        local q = st.q[i]
        if q.on and #q.u > 0 then
            local whole = 9900
            for _, sea in ipairs(seas) do
                if sea.oc == q.ocean then
                    whole = sea.ratio
                end
            end
            if q.kind == MAIN then
                main_step(st, q, s, now, whole)
            elseif q.kind == HUNT then
                hunt_step(st, q, s, now)
            elseif q.kind == STRIKE then
                strike_step(st, q, s, now)
            else
                scout_step(st, q, s, now, look)
            end
        end
    end
    if now - (st.log_t or 0) >= 60000 then
        st.log_t = now
        sq_log(st, seas)
    end
end

return M

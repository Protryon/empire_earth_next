-- Age of AI port: upstream dll/ai/airguard.c (the anti-air buildings, the AI's own air force and its share of the
-- military, the fighters' roles, the intercepts and the sweeps, the air bases; [ai_base] air_force_share = 25%,
-- fighter_sweeps = 1, air_by_worth = 1; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI
-- contributors). require("ageofai.airguard"), stepped from main.lua's step after ageofai.air.
--
-- It lives BESIDE ageofai.air (airstrike.c), it does not replace it: upstream's wing is the bombers' (airstrike.c
-- holds and trains them, up to AF_WING_MAX, and flies them at a position), airguard's are the fighters - the air
-- fighters (a_fighter*, the enemy's aircraft only), the fighter/bombers (a_strafe_fighter*, aircraft and soldiers and
-- citizens in the open) and the anti-tank planes (a_at_fighter*, the armour) - plus the anti-air buildings and the
-- intercepts. The two share one thing, the Airports: each asks for one a minute at most, and airguard yields to the
-- wing when air.lua has just ordered one (memory.air.airport_t); the share's bomber count is published for the wing
-- in memory.airguard.bombers (air.lua's train() may read it: the wiring is in the report; it is not needed for the
-- guard). The switch: S.air_force_share = 0 leaves the guard alone (fighters only against enemy aircraft seen, as
-- upstream without the share); air.lua's wing is S.air_strike and independent.
--
-- Every AG_EVERY from the fighters' epochs (view.epoch >= 9, World War I; upstream current_epoch + 1 >= 10):
--   AA BUILDINGS - while enemy aircraft were seen near its towns in the last AG_SEEN: as many as anti_air wants (1
--     and one for every 4 of the anti-air it wants, 4 at most), the newest anti-air building it can build and pay
--     (budget.build), by the drop site nearest to where the aircraft were - laid at once upstream (hook_place), here
--     through the build verb's own placement (budget.build = the priority list's building account).
--     STRONG ANTI-AIR (aa_strong, off upstream as shipped; defence.c aa_strong): while its air threat is real (aircraft
--     seen near its towns in AG_SEEN, or an enemy Airport it knows) what it still lacks of the block of AA buildings is
--     published for the priority list's FIRST row - memory.airguard.aa_hold (its cost, held from every other spender)
--     and aa_urgent - and its AA types wait for no row (priority.lua's S_AIR). Nothing is published with no aircraft
--     known. It trains no land AA (upstream's AA units and their maker are a gap: see the report).
--   THE AIR FORCE'S SIZE (air_force_share) - of its military (land soldiers, fighters, bombers) that share is
--     aircraft: fighters as many as the enemy's fighters known and AG_SPARE more, else AFS_FIGHTERS (40%) of it; the
--     rest bombers (the wing's, air.lua: memory.airguard.bombers); an Airport for every AFS_PORT_HOLDS (15) of them,
--     asked of the build verb at the forward point when there is none near the fighting. The share by worth
--     (air_by_worth): each part scaled by what it brings back lately (the killed for the lost, from a fifth to
--     twice) - the died events, by the aircraft that did the killing.
--   FIGHTERS - while enemy aircraft were seen: a fighter for every one and AG_SPARE more, AG_PER_AIRPORT an Airport
--     at most; with the share, up to its fighters' part as far as the Airports have room beside the bombers. AFS_BATCH
--     an Airport an AG_TRAIN_EVERY, the strongest of the role it lacks (air fighters against the enemy's aircraft
--     first, then fighter/bombers), paid as a priority order (budget.order) through ai.train at its Airports.
--   INTERCEPT - enemy aircraft it sees within AG_REACH of one of its buildings: every ready fighter (landed, rearmed,
--     AG_READY_PCT of its hit points) is sent at the nearest of them it can hurt (ai.can_hurt), the air fighters
--     first; one on its way is left alone AG_AGAIN. With the share only at a target it comes back from (its anti-air
--     test, below).
--   THE LEASH (with the share) - a fighter on an intercept AG_LEASH tiles beyond AG_REACH of its buildings is called
--     back to its nearest Airport.
--   THE SWEEPS (fighter_sweeps, with the share) - the ready fighters beyond the guard (the enemy's aircraft near its
--     towns and AG_SPARE stay for the intercept) go in flights of SW_FLIGHT from one Airport:
--       HUNT - one role at a time, at what its type can hurt where its anti-air can't bring the weakest of the flight
--              down: the most worth (an aircraft 600, a citizen 120, a land unit 400 of its cost) for the way, within
--              its reach (SW_REACH of its flight time), one flight a cell (SW_CELL);
--       LOOK - none of them: a fighter flies over the enemy's ground not looked at in SW_LOOK_MS - the enemy's
--              building remembered longest (the engine's memory has no "looked at" time: a deviation). One a look.
--     A fighter on a sweep is its own until it is back in its Airport, idle, or out SW_RUN_MS (then called home).
--
-- ENGINE GAPS (documented in the report, not ported): the Airport commands (EEMSetAirportCommand, EEMReturnToBase,
-- EEMScrambleAircraft are not in the AI's Lua API: docs/AI_LUA_UNITS.md §3.3 has "ai.air(...) - stage L6 or later"),
-- and a unit in a container cannot be ordered at all (ai.attack / ai.move refuse an aircraft parked in its Airport:
-- EECPUnitsOwnUnits' cannot_do). The port's stand-in: the module orders the airborne planes itself and, for a flight
-- that is parked, sets the Airport's rally at the same place (ai.rally: the game's airport order, which fills the
-- Airport's queue and launches its aircraft); an Airport the bomber wing is based at is left to air.lua. So:
-- - THE AIR BASES (airbase_tick): an aircraft cannot be rebased - a forward Airport shortens the reach only of the
--   planes trained there, so the module asks for the forward Airport and trains at the one nearest the fighting;
--   the parked planes stay home;
-- - the leash is a move home to the nearest Airport instead of a rebase (the plane lands at its own base);
-- - sw_survives' picture (airstrike.c dct, the anti-air damage over the sortie) is a count of the enemy anti-air
--   seen or remembered within AA_R of the target (2 or more: the target is too well held).
-- Deviations of the port: the enemy's fighters known are the engine's memory of its fighter types (no dct picture);
--   the killed-for-lost worth is from the step's died events with a 10-minute halving (returns.c is not ported); the
--   anti-air buildings go through the build verb's placement instead of hook_place; the log lines are a summary a
--   minute; the sent/sweep lists hold 64 a kind (upstream 256).
local S = require("ageofai.settings")
local V = require("lib.view")
local G = require("lib.groups")    -- the force's role (where it meets the enemy, combat_at)
local AIR = require("ageofai.air")   -- is_aa / is_bomber, the wing's airport timer (airstrike.c)
local PL = require("ageofai.plans")   -- [plans.c] the air-force leaning

local M = {}

-- upstream's defines
local AFS_FIGHTERS = 40          -- percent of the share that is fighters (0.4 with the worth off)
local AFS_BATCH = 2
local AFS_MAX = 6
local AFS_PORT_HOLDS = 15
local SW_FLIGHT = 4
local SW_RUN_MS = 150000
local SW_CELL = 8
local SW_TRIES = 48
local SW_REACH = 40              -- percent of the flight time's reach (0.4)
local SW_LOOK_MS = 300000
local AG_EVERY = 5000
local AG_SEEN = 600000
local AG_BUILD_EVERY = 60000
local AG_TRAIN_EVERY = 30000
local AG_BATCH = 2
local AG_SPARE = 2
local AG_PER_AIRPORT = 4
local AG_REACH = 18
local AG_READY_PCT = 60
local AG_AGAIN = 20000
local AG_LEASH = 6
local AG_FIGHTERS = 64           -- the sent and sweep records kept (upstream 256)
local AG_FOES = 64
local AG_AIRPORT_EVERY = 60000
local FIGHTER_EPOCH = 9          -- World War I, 0-based (upstream: current_epoch + 1 >= 10)

-- the fighter roles (ag_types): each epoch's air fighter (aircraft only), fighter/bomber (aircraft and land units),
-- and, World War II to the Modern Age, an anti-tank plane (the armour). Carrier planes are the carriers'.
M.NONE, M.AIR, M.STRAFE, M.AT = 0, 1, 2, 3
M.ROLE_NAMES = {[0] = "-", "air fighters", "fighter/bombers", "anti-tank planes"}
local role_cache = {}
function M.role(t)
    if t == nil then
        return M.NONE
    end
    local r = role_cache[t]
    if r ~= nil then
        return r
    end
    r = M.NONE
    if string.sub(t, 1, 12) == "a_at_fighter" then
        r = M.AT
    elseif string.sub(t, 1, 16) == "a_strafe_fighter" then
        r = M.STRAFE
    elseif string.sub(t, 1, 9) == "a_fighter" then
        r = M.AIR
    end
    role_cache[t] = r
    return r
end

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

-- a class by name, cached per game (ai.class is a charge a call)
local cls_cache = {}
local function cls(t)
    local c = cls_cache[t]
    if c == nil then
        c = ai.class(t) or {}
        cls_cache[t] = c
    end
    return c
end

-- what a soldier is worth to train (soldier_score): hit points x attack over its price
local function score(t)
    local c = cls(t)
    local cost = V.sum(c.cost or {})
    if cost <= 0 or (c.attack or 0) <= 0 then
        return 0
    end
    return (c.hp or 1) * (c.attack or 0) * 100 // cost
end

-- the pilot's craft is ready: no goal (landed and rearmed) and AG_READY_PCT of its hit points
local function ready(u)
    local parked = u.inside ~= nil or u.action == nil or u.action == "idle" or u.action == "inside"
    return parked and u.hp * 100 >= (u.hp_max or u.hp) * AG_READY_PCT
end

local function sent_at(s, id)
    for k, r in ipairs(s.sent) do
        if r.id == id then
            return k
        end
    end
    return nil
end

local function on_sweep(s, id)
    for k, r in ipairs(s.sw) do
        if r.id == id then
            return k
        end
    end
    return nil
end

-- the intercept records kept only while they mean something (3 AG_AGAIN, upstream's ring of slots)
local function sent_watch(s, now)
    local keep = {}
    for _, r in ipairs(s.sent) do
        if r.t > 0 and now >= r.t and now - r.t < 3 * AG_AGAIN then
            keep[#keep + 1] = r
        end
    end
    s.sent = keep
end

-- the state: memory.airguard
local function state(mem)
    mem.airguard = mem.airguard or {sent = {}, sw = {}, ours = {}}
    local s = mem.airguard
    s.sent = s.sent or {}
    s.sw = s.sw or {}
    s.ours = s.ours or {}
    return s
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE AIR'S WORTH (returns.c rt_worth, from the step's died events: the value killed for the value lost lately)

local function worth_note(s, events, now)
    if not S.air_by_worth then
        return
    end
    s.killed = s.killed or {}
    s.lost = s.lost or {}
    for _, e in ipairs(events or {}) do
        if e.kind == "died" then
            if e.killer ~= nil and e.type ~= nil and s.ours[e.killer] ~= nil then
                local k = s.ours[e.killer]
                s.killed[k] = (s.killed[k] or 0) + math.max(V.sum(cls(e.type).cost or {}), 1)
            end
            if s.ours[e.unit] ~= nil then
                local k = s.ours[e.unit]
                s.lost[k] = (s.lost[k] or 0) + math.max(V.sum(cls(e.type or "").cost or {}), 1)
            end
        end
    end
    -- the window: the last 10 minutes weigh most, an older kill and loss fade by half every 10
    if (s.worth_t or 0) + 600000 <= now then
        s.worth_t = now > 0 and now or 1
        for k = 1, 2 do
            s.killed[k] = (s.killed[k] or 0) // 2
            s.lost[k] = (s.lost[k] or 0) // 2
        end
    end
end

-- what its fighters (kind 1) or bombers (kind 2) bring back for what they lose, a fifth to twice (rt_worth)
local function worth(s, kind)
    if not S.air_by_worth then
        return 100
    end
    local k, l = (s.killed or {})[kind] or 0, (s.lost or {})[kind] or 0
    local w = (k + 50) * 100 // (l + 50)
    if w < 20 then
        w = 20
    elseif w > 200 then
        w = 200
    end
    return w
end

-- ---------------------------------------------------------------------------------------------------------------
-- The own air and the airports

-- its fighters (records by id, the ids in order) and its bombers' count; every fighter taken: the engine's air
-- subgroups never order it (upstream's HELD: unheld, the game's groups flew them into the enemy's anti-air)
local function roster(s)
    local fighters, ids, bombers = {}, {}, 0
    local have = {[M.NONE] = 0, 0, 0, 0}
    local take = {}
    for _, id in ipairs(ai.units{category = "air", limit = 200}) do
        local u = ai.unit(id)
        if u ~= nil then
            if M.role(u.type) ~= M.NONE then
                have[M.role(u.type)] = have[M.role(u.type)] + 1
                if #ids < AG_FIGHTERS then
                    fighters[id] = u
                    ids[#ids + 1] = id
                    s.ours[id] = 1
                    if u.held ~= "lua" then
                        take[#take + 1] = id
                    end
                end
            elseif AIR.is_bomber(u.type) then
                bombers = bombers + 1
                s.ours[id] = 2
                if u.base ~= nil then
                    s.bomber_ports = s.bomber_ports or {}
                    s.bomber_ports[u.base] = true   -- the wing's: no rally of the airport for a fighter launch
                end
            end
        end
    end
    if #take > 0 then
        ai.take(take)
    end
    return fighters, ids, bombers, have
end

-- the finished Airports (ids) and those still going up
local function airports()
    local built, building = {}, 0
    for _, id in ipairs(ai.units{type = "b_airport", limit = 64}) do
        local u = ai.unit(id)
        if u ~= nil then
            if u.built then
                built[#built + 1] = id
            else
                building = building + 1
            end
        end
    end
    return built, building
end

-- its buildings' tiles (the intercepts' and the leash's near test; walls left out) and its anti-air buildings' count
local function buildings()
    local out, aa = {}, 0
    for _, id in ipairs(ai.units{category = "building", limit = 128}) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil then
            aa = aa + (AIR.is_aa(u.type) and 1 or 0)
            if string.sub(u.type, 1, 7) ~= "b_wall_" and string.sub(u.type, 1, 7) ~= "b_gate_" then
                out[#out + 1] = {x = u.x, y = u.y}
            end
        end
    end
    return out, aa
end

local function building_near(bld, x, y, r)
    for _, b in ipairs(bld) do
        if d2(b.x, b.y, x, y) <= r * r then
            return true
        end
    end
    return false
end

-- the enemy anti-air seen or remembered near a tile (sw_survives' picture, counted: 2 or more hold it)
local AA_R = 14
local function aa_near(x, y, r)
    local n = 0
    for _, id in ipairs(ai.seen{enemy = true, near = {x, y, r}, limit = 8}) do
        local u = ai.unit(id)
        if u ~= nil and AIR.is_aa(u.type) then
            n = n + 1
        end
    end
    for _, m in ipairs(ai.memory{buildings = true, near = {x, y, r}, limit = 8}) do
        if AIR.is_aa(m.type) then
            n = n + 1
        end
    end
    return n
end

-- where its force meets the enemy next (stations.c sn_need_at): the push's destination, or the force's place out
local function combat_at(memory)
    local army = memory.army or {}
    local force = G.role(memory, "force")
    if army.out and force ~= nil and force.task == "attack" and (force.x or -1) >= 0 then
        return force.x, force.y
    end
    return nil
end

-- the enemy's fighters known (THE PICTURE's nf, here the memory's records of its fighter types)
local function foe_fighters(now)
    local n = 0
    for _, r in ipairs(ai.memory{since = math.max(now - 600000, 0)}) do
        if not r.building and M.role(r.type) ~= M.NONE then
            n = n + 1
            if n >= 32 then
                break
            end
        end
    end
    return n
end

-- ---------------------------------------------------------------------------------------------------------------
-- AA BUILDINGS (airguard_tick's first block): the newest anti-air building it can pay, by the aircraft

-- the newest anti-air building the game offers it now (aa_type_of KIND_AA_BUILDING: the newest epoch, the last of a
-- tie) - its view.costs entry, or nil
local function aa_newest(view)
    local best, epoch = nil, -2
    for _, c in ipairs(view.costs) do
        if c.building and AIR.is_aa(c.type) then
            local e = cls(c.type).epoch or -1
            if e >= epoch then
                best, epoch = c, e
            end
        end
    end
    return best
end
local function aa_building(view, budget)
    local best = aa_newest(view)
    if best == nil then
        return nil
    end
    for _, r in ipairs(V.resources) do
        if (best.cost[r] or 0) > ((budget or {})[r] or 0) then
            return nil   -- the priority list's building account cannot pay it
        end
    end
    return best.type
end

local function aa_tick(view, memory, s, budget, bld, now)
    -- the enemy aircraft near its towns (defence.c anti_air's g_aa_st): counted, remembered AG_SEEN
    local air, ax, ay = 0, nil, nil
    for _, id in ipairs(ai.seen{enemy = true, category = "air", limit = AG_FOES}) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil and (building_near(bld, u.x, u.y, AG_REACH) or #bld == 0) then
            air = air + 1
            if ax == nil then
                ax, ay = u.x, u.y
            end
        end
    end
    if air > 0 then
        s.aa_seen_t, s.aa_seen_n, s.aa_seen_x, s.aa_seen_y = now > 0 and now or 1, air, ax, ay
    end
    local seen = (s.aa_seen_t or 0) > 0 and now - s.aa_seen_t < AG_SEEN
    local want = 2 + ((seen and s.aa_seen_n or 0) + 1) // 2
    local want_b = seen and s.aa_seen_n > 0 and 1 + (math.min(want, 16) // 4) or 0
    if want_b > 4 then
        want_b = 4
    end
    if (s.have_aa or 0) >= want_b or (s.build_t or 0) + AG_BUILD_EVERY > now then
        return
    end
    local bt = aa_building(view, budget)
    if bt == nil then
        return
    end
    s.build_t = now > 0 and now or 1
    -- by the drop site nearest to where the aircraft were (hook_place upstream: the placement here is the verb's)
    local dx, dy = s.aa_seen_x, s.aa_seen_y
    if dx ~= nil then
        local _, _, sx, sy = ai.drop_site(dx, dy)
        if sx ~= nil and sx >= 0 then
            dx, dy = sx, sy
        end
    end
    local x, y = nil, nil
    if dx ~= nil then
        x, y = ai.find_site(bt, dx, dy, {min = 0, max = 8})
    end
    s.aa_orders = (s.aa_orders or 0) + 1
    if x ~= nil then
        return {verb = "build", type = bt, near = "tile", x = x, y = y}
    end
    return {verb = "build", type = bt, near = "town"}
end

-- STRONG ANTI-AIR ([ai_base] aa_strong; defence.c aa_strong, priority.c's row "air"): while its air threat is real -
-- enemy aircraft seen near its towns in the last AG_SEEN, or an enemy Airport it knows - the anti-air it still wants is
-- the priority list's FIRST row: its cost is held from every other spender (memory.airguard.aa_hold) and its own AA
-- types wait for no row (priority.lua marks them S_AIR). Upstream holds a round of AAS_TRAIN AA units, the block still
-- missing and the building that makes the AA units; the port trains no AA unit (a gap named in the report), so the row
-- holds the block of AA buildings it still lacks. Nothing is published while no aircraft are known: the row is inert.
local AAS_BLOCK = 2      -- a block: AAS_BLOCK while the threat is below AAS_BIG, else its double (upstream 2 / 4)
local AAS_BIG = 8
local function aa_publish(view, s, have_aa, air)
    s.aa_urgent, s.aa_hold = nil, nil
    if not S.aa_strong then
        return
    end
    local port = #ai.memory{buildings = true, type = "b_airport", limit = 1} > 0   -- an enemy Airport it knows
    if air <= 0 and not port then
        return
    end
    s.aa_urgent = 1   -- the threat is real: its AA types wait for no row from now on
    local block = air >= AAS_BIG and 2 * AAS_BLOCK or AAS_BLOCK
    local miss = block - (have_aa or 0)
    if port and miss < 1 then
        miss = 1   -- an airport known: static guns stand before the first wave
    end
    local c = aa_newest(view)
    if miss <= 0 or c == nil then
        return   -- the block stands (or it can build no AA): nothing to save for
    end
    local hold = {}
    for _, r in ipairs(V.resources) do
        hold[r] = (c.cost[r] or 0) * miss
    end
    s.aa_hold = hold
end

-- ---------------------------------------------------------------------------------------------------------------
-- The fighters trained (FIGHTERS, THE AIR FORCE'S SIZE)

local function train(view, s, orders, budget, ap, ap_near, bombers, nm, want_f, sz_f, air, have, now)
    if nm >= want_f or (s.train_t or 0) + AG_TRAIN_EVERY > now then
        return
    end
    s.train_t = now > 0 and now or 1
    if #ap == 0 then
        return
    end
    -- the roles: air fighters against the enemy's aircraft (all of them without the share), the rest fighter/bombers
    local air_want = sz_f > 0 and math.min(air + AG_SPARE, want_f) or want_f
    local role = have[M.AIR] < air_want and M.AIR or M.STRAFE
    local best = nil
    for pass = 1, 2 do
        for _, c in ipairs(view.costs) do
            local r = M.role(c.type)
            if not c.building and r ~= M.NONE and r ~= M.AT and (r == role) == (pass == 1) then
                if best == nil or score(c.type) > score(best) then
                    best = c.type
                end
            end
        end
        if best ~= nil then
            break
        end
    end
    if best == nil then
        return
    end
    local batch = sz_f > nm and #ap * AFS_BATCH or AG_BATCH
    if batch > AFS_MAX then
        batch = AFS_MAX
    end
    local n = math.min(want_f - nm, batch)
    -- paid as a priority order (the stock above the epoch reserve: budget.order), [priority.c]
    local cost = V.cost(view, best)
    if cost ~= nil then
        for _, r in ipairs(V.resources) do
            if (cost.cost[r] or 0) > 0 then
                n = math.min(n, ((budget.order or budget.civil or {})[r] or 0) // cost.cost[r])
            end
        end
    end
    if n <= 0 then
        return
    end
    -- at the Airport nearest the fighting when it is known (the air bases' part this engine can express: a plane
    -- trained there is based there), else at all of them
    local where = ap
    if ap_near ~= nil then
        where = {ap_near}
    end
    local _, _, got = ai.train(where, best, n)
    if got > 0 then
        s.trained = (s.trained or 0) + got
        print(string.format("airguard trained n=%d type=%s role=%s have=%d want=%d airports=%d forward=%s",
            got, best, M.ROLE_NAMES[role], nm, want_f, #ap, tostring(ap_near)))
    else
        s.refused = (s.refused or 0) + 1
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE AIR BASES: the forward Airport (no rebase in this engine: the planes trained there are the ones forward)

-- the Airport nearest where the force meets the enemy (stations.c sn_need_at): out of danger and with no enemy
-- soldier near it (in no territory being lost)
local function forward(ap, memory)
    local fx, fy = combat_at(memory)
    if fx == nil then
        return nil, nil, nil, nil
    end
    local fwd, fd = nil, nil
    for _, id in ipairs(ap) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil and ai.danger(u.x, u.y) <= 0
                and #ai.seen{enemy = true, category = "soldier", near = {u.x, u.y, 10}, limit = 1} == 0 then
            local d = d2(u.x, u.y, fx, fy)
            if fd == nil or d < fd then
                fwd, fd = id, d
            end
        end
    end
    return fwd, fd, fx, fy
end

local function airbase(view, memory, s, orders, budget, ap, fwd, fd, fx, fy, now)
    if fx == nil or (s.port_t or 0) + AG_AIRPORT_EVERY > now then
        return
    end
    if fd ~= nil and isqrt(fd) <= 20 then
        return   -- an Airport at the fighting already
    end
    -- one more Airport at the forward point, one a minute (air.lua's wing asks for its own too: it goes first)
    if memory.air ~= nil and (memory.air.airport_t or 0) + AG_AIRPORT_EVERY > now then
        return
    end
    local entry = V.buildings(view, "b_airport")
    if entry ~= nil and entry.queued + entry.unfinished > 0 then
        return
    end
    local cost = V.cost(view, "b_airport")
    if cost == nil then
        return
    end
    for _, r in ipairs(V.resources) do
        if (cost.cost[r] or 0) > ((budget.build or {})[r] or 0) then
            return
        end
    end
    local x, y = ai.find_site("b_airport", fx, fy, {min = 0, max = 12})
    s.port_t = now > 0 and now or 1
    if x ~= nil then
        orders[#orders + 1] = {verb = "build", type = "b_airport", near = "tile", x = x, y = y}
    else
        orders[#orders + 1] = {verb = "build", type = "b_airport", near = "town"}
    end
    s.ports_asked = (s.ports_asked or 0) + 1
    print(string.format("airguard airport at=%s,%s forward=%d,%d have=%d", tostring(x), tostring(y), fx, fy, #ap))
end

-- ---------------------------------------------------------------------------------------------------------------
-- INTERCEPT and THE LEASH

-- the order to a flight: an attack (or a move) per plane where the API takes an airborne one (ai.attack / ai.move);
-- a fighter parked in its Airport is a container unit and the unit commands refuse it (docs/AI_LUA_UNITS §3.3, the
-- missing airport commands), so its Airport's rally is set at the same place - ai.rally fills an airport's queue and
-- its aircraft launch (the game's own airport order, which this engine does expose). An Airport the bomber wing is
-- based at is left alone: air.lua orders those itself.
local function order(s, fighters, ids, tgt, x, y)
    local flying, parked, base = {}, {}, nil
    for _, id in ipairs(ids) do
        local u = fighters[id]
        base = base or u.base
        if u.inside ~= nil then
            parked[#parked + 1] = id
        else
            flying[#flying + 1] = id
        end
    end
    local n = 0
    if #flying > 0 then
        local _, _, got
        if tgt ~= nil then
            _, _, got = ai.attack(flying, tgt)
        else
            _, _, got = ai.move(flying, x, y, {forced = false})
        end
        n = n + (got or 0)
    end
    if #parked > 0 and base ~= nil and not (s.bomber_ports or {})[base] then
        local _, _, got = ai.rally(base, x, y)
        if (got or 0) > 0 then
            n = n + #parked
            s.launched = (s.launched or 0) + #parked
        end
    end
    if n == 0 then
        s.refused = (s.refused or 0) + 1
    end
    return n
end

-- the enemy aircraft it may intercept at, its anti-air counted once a foe (with the share: sw_survives' test)
local function foes_ready(bld, share, now)
    local foes = {}
    for _, id in ipairs(ai.seen{enemy = true, category = "air", limit = AG_FOES}) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil and building_near(bld, u.x, u.y, AG_REACH) then
            foes[#foes + 1] = u
        end
    end
    if share and #foes > 0 then
        for k = 1, math.min(#foes, 8) do
            foes[k].aa = aa_near(foes[k].x, foes[k].y, AA_R)
        end
    end
    return foes
end

local function intercept(s, fighters, ids, foes, now)
    local sent = 0
    for pass = 1, 2 do   -- the air fighters first, then the others
        for _, id in ipairs(ids) do
            local u = fighters[id]
            if ((M.role(u.type) == M.AIR) == (pass == 1)) and ready(u) then
                local k = sent_at(s, id)
                if k == nil or not (now >= s.sent[k].t and now - s.sent[k].t < AG_AGAIN) then
                    local tg, bd = nil, nil
                    for _, f in ipairs(foes) do
                        local d = d2(f.x, f.y, u.x, u.y)
                        if (tg == nil or d < bd) and ai.can_hurt(u.type, f.type)
                                and ((S.air_force_share or 0) <= 0 or (f.aa or 0) < 2) then
                            tg, bd = f, d
                        end
                    end
                    if tg ~= nil then
                        local n = order(s, fighters, {id}, tg.id, tg.x, tg.y)
                        if n > 0 then
                            if k == nil then
                                if #s.sent < AG_FIGHTERS then
                                    s.sent[#s.sent + 1] = {id = id, t = now > 0 and now or 1}
                                end
                            else
                                s.sent[k].t = now > 0 and now or 1
                            end
                            sent = sent + 1
                        end
                    end
                end
            end
        end
    end
    if sent > 0 then
        s.orders = (s.orders or 0) + sent
        s.sorties = (s.sorties or 0) + 1
    end
end

-- a fighter on an intercept AG_LEASH tiles beyond AG_REACH of its buildings is called back to its nearest Airport
local function leash(s, ap, bld, now)
    if (S.air_force_share or 0) <= 0 or #ap == 0 then
        return
    end
    for k, r in ipairs(s.sent) do
        if now >= r.t and now - r.t < 3 * AG_AGAIN then
            local u = ai.unit(r.id)
            if u ~= nil and u.inside == nil and u.x ~= nil then
                if not building_near(bld, u.x, u.y, AG_REACH + AG_LEASH) then
                    local bp, bd = nil, nil
                    for _, id in ipairs(ap) do
                        local a = ai.unit(id)
                        if a ~= nil and a.x ~= nil then
                            local d = d2(a.x, a.y, u.x, u.y)
                            if bd == nil or d < bd then
                                bp, bd = a, d
                            end
                        end
                    end
                    if bp ~= nil then
                        ai.move({r.id}, bp.x, bp.y, {forced = true})
                        r.t = 0   -- once
                        s.leashed = (s.leashed or 0) + 1
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE SWEEPS

-- a flight at the most worth its type can hurt by cell, else a look; one flight a cell, one a look for the look
local function sweep(s, fighters, ids, ap, now, looked)
    local free = {}
    for _, id in ipairs(ids) do
        local u = fighters[id]
        if ready(u) and sent_at(s, id) == nil and on_sweep(s, id) == nil then
            free[#free + 1] = id
        end
    end
    if #free == 0 then
        return looked
    end
    -- by the Airport each is based at (u.base: the engine's home base); the soundest first
    local groups, order = {}, {}
    for _, id in ipairs(free) do
        local u = fighters[id]
        local base = u.base
        if base == nil then
            local bp, bd = nil, nil
            for _, a in ipairs(ap) do
                local r = ai.unit(a)
                if r ~= nil and r.x ~= nil then
                    local d = d2(r.x, r.y, u.x, u.y)
                    if bd == nil or d < bd then
                        bp, bd = a, d
                    end
                end
            end
            base = bp or 0
        end
        if groups[base] == nil then
            groups[base] = {}
            order[#order + 1] = base
        end
        local g = groups[base]
        g[#g + 1] = id
    end
    for _, base in ipairs(order) do
        local g = groups[base]
        if #g > 0 then
            -- the flight: up to SW_FLIGHT, the soundest first
            table.sort(g, function(a, b) return fighters[a].hp > fighters[b].hp end)
            local flight = {}
            for i = 1, math.min(#g, SW_FLIGHT) do
                flight[#flight + 1] = g[i]
            end
            local ft = fighters[flight[1]].type
            local c = cls(ft)
            local home = base ~= 0 and ai.unit(base) or nil
            local hx = home ~= nil and home.x or fighters[flight[1]].x
            local hy = home ~= nil and home.y or fighters[flight[1]].y
            if hx ~= nil then
                -- SW_REACH of its flight time's way out and back (ai.class's flight_time in ms, speed in cm a second)
                local fms = c.flight_time or 0
                local reach = fms > 0 and (fms * (c.speed or 0) // 100000) * SW_REACH // 100 or 60
                -- HUNT: what this type can hurt, by cell, the most worth for the way
                local cells, list = {}, {}
                for _, id in ipairs(ai.seen{enemy = true, near = {hx, hy, reach}, limit = 64}) do
                    local e = ai.unit(id)
                    if e ~= nil and e.x ~= nil and ai.can_hurt(ft, e.type) then
                        local key = tostring(e.x // SW_CELL) .. "," .. tostring(e.y // SW_CELL)
                        local cell = cells[key]
                        if cell == nil then
                            cell = {x = e.x // SW_CELL * SW_CELL + SW_CELL // 2, y = e.y // SW_CELL * SW_CELL + SW_CELL // 2,
                                    v = 0, bv = 0, tgt = nil}
                            cells[key] = cell
                            list[#list + 1] = cell
                        end
                        local ec = cls(e.type)
                        local value = e.category == "air" and math.min(V.sum(ec.cost or {}), 600)
                                or e.category == "citizen" and 120 or math.min(V.sum(ec.cost or {}), 400)
                        cell.v = cell.v + value
                        if value > cell.bv then
                            cell.bv, cell.tgt = value, e
                        end
                    end
                end
                -- one flight a cell (not a cell a sweep is on already), the best worth for the way first
                local best, bpct = nil, nil
                for _ = 1, math.min(#list, SW_TRIES) do
                    local cand, cd, cv = nil, nil, nil
                    for _, cell in ipairs(list) do
                        if cell.taken == nil then
                            local busy = false
                            for _, w in ipairs(s.sw) do
                                if w.kind == 1 and w.at_x // SW_CELL == cell.x // SW_CELL and w.at_y // SW_CELL == cell.y // SW_CELL then
                                    busy = true
                                end
                            end
                            local dc = isqrt(d2(cell.x, cell.y, hx, hy))
                            if not busy and dc <= reach then
                                local v = cell.v * 100 // (30 + dc)
                                if cv == nil or v > cv then
                                    cand, cd, cv = cell, dc, v
                                end
                            end
                        end
                    end
                    if cand == nil then
                        break
                    end
                    -- it comes back: its anti-air there and on the way (sw_survives; the picture is a count here)
                    if aa_near(cand.x, cand.y, 12) < 2 then
                        best, bpct = cand, cd
                        break
                    end
                    s.risky = (s.risky or 0) + 1
                    cand.taken = true
                end
                if best ~= nil and best.tgt ~= nil then
                    local n = order(s, fighters, flight, best.tgt.id, best.x, best.y)
                    if n > 0 then
                        for i = 1, n do
                            local id = flight[i]
                            if id ~= nil and #s.sw < AG_FIGHTERS then
                                s.sw[#s.sw + 1] = {id = id, tgt = best.tgt.id, home = base, t = now > 0 and now or 1,
                                                    at_x = best.x, at_y = best.y, kind = 1}
                            end
                        end
                        s.runs = (s.runs or 0) + 1
                        s.sw_sent = (s.sw_sent or 0) + n
                        print(string.format("airguard sweep n=%d at=%d,%d worth=%d from=%d,%d d=%d role=%s", n, best.x,
                            best.y, best.v, hx, hy, bpct or -1, M.ROLE_NAMES[M.role(ft)]))
                    end
                elseif not looked then
                    -- LOOK: the enemy's ground not looked at in SW_LOOK_MS (its building remembered longest)
                    local old, od = nil, nil
                    for _, m in ipairs(ai.memory{buildings = true, limit = 64}) do
                        local ago = now - (m.seen or 0)
                        if ago >= SW_LOOK_MS and d2(m.x, m.y, hx, hy) <= reach * reach then
                            if od == nil or ago > od then
                                old, od = m, ago
                            end
                        end
                    end
                    if old ~= nil then
                        local n = order(s, fighters, {flight[1]}, nil, old.x, old.y)
                        if n > 0 then
                            if #s.sw < AG_FIGHTERS then
                                s.sw[#s.sw + 1] = {id = flight[1], tgt = 0, home = base, t = now > 0 and now or 1,
                                                    at_x = old.x, at_y = old.y, kind = 2}
                            end
                            s.looks = (s.looks or 0) + 1
                            looked = true
                        end
                    end
                end
            end
        end
    end
    return looked
end

-- the sweeps out looked at: lost, back, or out too long (called home)
local function sw_watch(s, now)
    local keep = {}
    for _, w in ipairs(s.sw) do
        local u = ai.unit(w.id)
        if u == nil then
            s.sw_lost = (s.sw_lost or 0) + 1
        else
            local out = now - w.t
            local done = out >= 10000 and (u.inside ~= nil or u.action == "idle")
            if not done and out >= SW_RUN_MS then
                local home = w.home ~= 0 and ai.unit(w.home) or nil
                if home ~= nil and home.x ~= nil and u.inside == nil then
                    ai.move({w.id}, home.x, home.y, {forced = true})
                end
                s.sw_back = (s.sw_back or 0) + 1
                done = true
            end
            if not done then
                keep[#keep + 1] = w
            end
        end
    end
    s.sw = keep
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (airguard_tick): every AG_EVERY

function M.step(view, memory, orders, budget, events)
    local s = state(memory)
    local now = view.time
    worth_note(s, events, now)
    if (s.t or 0) > 0 and now - s.t < AG_EVERY then
        return
    end
    s.t = now > 0 and now or 1
    if view.epoch < FIGHTER_EPOCH then
        return
    end
    local fighters, ids, bombers, have = roster(s)
    local ap, ap_building = airports()
    local bld, have_aa = buildings()
    s.have_aa = have_aa
    local land = #ai.units{category = "soldier", armed = true, limit = 400}
    -- AA BUILDINGS
    local aa_order = aa_tick(view, memory, s, budget.build, bld, now)
    if aa_order ~= nil then
        orders[#orders + 1] = aa_order
    end
    local air = (s.aa_seen_t or 0) > 0 and now - s.aa_seen_t < AG_SEEN and (s.aa_seen_n or 0) or 0
    -- STRONG ANTI-AIR: what the priority list's first row holds while the threat is real (priority.lua reads it)
    aa_publish(view, s, have_aa, air)
    -- THE AIR FORCE'S SIZE and the Airports for it
    local nm = #ids
    local pct = S.air_force_share or 0
    pct = math.floor(pct * PL.plan_f(memory, 0) + 0.5)
    if pct < 0 then
        pct = 0
    end
    local sz, sz_f, sz_b, holds = 0, 0, 0, AFS_PORT_HOLDS
    if pct > 0 then
        local cap = cls("b_airport").transport or 0
        if cap > 0 then
            holds = cap
        end
        local base = (land + nm + bombers) * pct // 100
        local fsh = AFS_FIGHTERS
        if S.air_by_worth then
            -- each part of the share by what it brings back (rt_worth): the fighters' and the bombers' worth
            local f = base * AFS_FIGHTERS * worth(s, 1) // 10000
            local b = base * (100 - AFS_FIGHTERS) * worth(s, 2) // 10000
            base = f + b
            fsh = base > 0 and f * 100 // base or AFS_FIGHTERS
        end
        sz = base
        local known = math.max(foe_fighters(now), air)
        sz_f = known > 0 and known + AG_SPARE or 0
        if sz_f < sz * fsh // 100 then
            sz_f = sz * fsh // 100
        end
        if sz_f > sz then
            sz_f = sz
        end
        sz_b = sz - sz_f
        s.bombers = sz_b
        s.ports = (sz + holds - 1) // holds
        s.room = #ap * holds - math.min(nm, sz_f)   -- the bombers' room beside the fighters it keeps
    else
        s.bombers, s.ports, s.room = 0, 0, 0
    end
    s.sz, s.sz_f, s.sz_b, s.land, s.airports, s.ap_building = sz, sz_f, sz_b, land, #ap, ap_building
    -- the Airport nearest the fighting (the air bases): the planes trained there are the ones forward
    local fwd, fd, fx, fy = forward(ap, memory)
    s.fwd = fwd
    s.fwd_d = fd ~= nil and isqrt(fd) or -1
    if pct > 0 and s.ports > #ap + ap_building then
        airbase(view, memory, s, orders, budget, ap, fwd, fd, fx, fy, now)
    end
    -- FIGHTERS: the guard's (the enemy aircraft near its towns and AG_SPARE), then the share's part
    local want_f = air > 0 and air + AG_SPARE or 0
    if want_f > #ap * AG_PER_AIRPORT then
        want_f = #ap * AG_PER_AIRPORT
    end
    if sz_f > want_f then
        local room = #ap * holds - bombers
        want_f = sz_f <= room and sz_f or (room > want_f and room or want_f)
    end
    s.want_f = want_f
    train(view, s, orders, budget, ap, s.fwd, bombers, nm, want_f, sz_f, air, have, now)
    -- INTERCEPT: the enemy aircraft within AG_REACH of its buildings
    local foes = foes_ready(bld, pct > 0, now)
    if #ids > 0 and #foes > 0 then
        intercept(s, fighters, ids, foes, now)
    end
    leash(s, ap, bld, now)
    sent_watch(s, now)
    -- THE SWEEPS: the ready beyond the guard
    sw_watch(s, now)
    if pct > 0 and S.fighter_sweeps ~= false then
        local keep = math.max(want_f, AG_SPARE)
        local free = {}
        for _, id in ipairs(ids) do
            local u = fighters[id]
            if ready(u) and sent_at(s, id) == nil and on_sweep(s, id) == nil then
                if keep > 0 and M.role(u.type) ~= M.STRAFE then
                    keep = keep - 1   -- the guard stays (the air fighters first)
                else
                    free[#free + 1] = id
                end
            end
        end
        if #free > 0 then
            sweep(s, fighters, free, ap, now, false)
        end
    end
    if (s.log_t or 0) + 60000 <= now and (nm > 0 or air > 0) then
        s.log_t = now > 0 and now or 1
        print(string.format("airguard fighters=%d/%d air=%d airports=%d/%d aa=%d | want=%d f=%d b=%d worth=%d/%d | sorties=%d leashed=%d swept=%d lost=%d back=%d trained=%d",
            nm, want_f, air, #ap, s.ports or 0, s.have_aa or 0, sz, sz_f, sz_b, worth(s, 1), worth(s, 2),
            s.sorties or 0, s.leashed or 0, s.sw_sent or 0, s.sw_lost or 0, s.sw_back or 0, s.trained or 0))
    end
end

return M

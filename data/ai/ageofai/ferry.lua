-- Age of AI port: upstream dll/ai/ferry.c (THE FERRY SERVICE: the citizens and soldiers across the water - the lands
-- and their free places, the pick-up and the meeting point, the boarding, the trips and their time limits by the way,
-- the transports gathered and held, the scouts across, thrift; UPSTREAM names the commit, the per-file map and the
-- licence - GPL-3.0, Grille and the Age of AI contributors). require("ageofai.ferry").
--
-- The player, 2026-11-17, on citizens and soldiers kept from the other lands: "citezen balacning acroos territories
-- dosnt work across water yet right? / or general reinforcment" - and yes to one service both use. The force's shares
-- and the citizens' work walk the land, but a territory across the water kept short of its share however many soldiers
-- stood spare at home. AS ON LAND (2026-11-18: "remove the limiters, the reinforcment/balancing should work like on
-- land, and if an ferry is bussy, aditional should be build"): every FY_LOOK (5 s), by land - a continent, an island:
--  - soldiers from a land over its share to one short of it (the shares are territories.lua's guards, its own model of
--    the stations' shares); those aboard or on their way count for the land they go to;
--  - citizens - idle ones first, then wood cutters - from a land with any to spare to one with free places at the
--    mines it wants more citizens on (and the trees, with idle ones at home) and a drop site of its there.
-- One trip a transport, however few go along; as many trips at once as it has transports for (FY_TRIPS). TURNS: the
-- kind that did not start the last trip goes first (a whole game of soldiers' trips while the citizens' free places
-- stood empty was one of A's faults).
-- A TRIP: the nearest free transport on a sea both lands touch (no landing's, no outpost ferry's, no other trip's;
-- empty) goes to the shore nearest the passengers' middle, they walk there meanwhile, board, and are put off at the
-- shore nearest where they are wanted; there they are the others' again. THE BOARDING: all aboard, or some aboard for
-- FY_LOAD (90 s) - off with those; nobody aboard by FY_BOARD (240 s, the way's with ferry_time_by_way) - given up. (The
-- landing's 9 in 10 after a minute / 3 minutes / half at least is lib.navy's board for the landing, not the ferry's.)
-- THE TIME BY THE WAY (ferry_time_by_way): the time to the pick-up and the time across are the way's (straight, or
-- through the point off the line when there is one) x FY_WAY_DETOUR at the transport's speed, plus FY_WAY_SLACK; the
-- fixed limits at the least (on the player's huge map every long trip was given up in 4 minutes, its transport still
-- 180 tiles off). THE CROSSING (crossing.lua, [ai_base] ferry_safety): the way rated - the damage expected before the
-- load is off against what the delivery is worth (a hot land's soldiers: the transport may be lost after; the shares
-- and the citizens: it must come through; into danger with a full load only) - and held at the shore, unloaded, when
-- the risk is too high. MORE TRANSPORTS (thrift): a transport ordered only for a trip that found none free on a sea
-- both its lands touch, at a dock on such a sea; none with no such dock (ferry_thrift; off upstream, on is the
-- player's measured call). HOME TO A DOCK (transport_park): a free transport with no order goes to the water by its
-- nearest finished dock out of danger, its sea's free transports spread over the docks in even shares. SCOUTS ACROSS:
-- a scout that asks (M.scout_ask, explore.lua is not wired) gets one transport to the other land.
--
-- PORT DEVIATIONS AND GAPS (the engine's, not upstream's):
--  - The lands are continents from ai.tile, not the game's 40-area split; the shares are territories.lua's guards and
--    the free places are the view's sites (the claim range's resource centres, with their room), not work.c's whole
--    map. A land without a site known shows no free places.
--  - The passengers are the free engine-held soldiers (the force's are army.lua's: no stations to give them across)
--    and the engine-held citizens (lib.verbs'); no bridgehead, push or prophet passengers (stations.c/holy.c).
--  - FY_REACH (a pick-up both the passengers and the transport reach, the walking areas of the game's pathing) has no
--    data here: the pick-up is the plain one (the shore nearest the passengers' middle).
--  - The meeting point (crossing.c cx_meet) is used by a reinforcement to a hot land that meets an earlier one's way;
--    a push's waves (GATHERED FIRST, BOARD TOGETHER, THE LAST WORD, FY_GATHER/FY_MEET/FY_TOGETHER) wait for stations.c.
--  - claims.lua holds the trip's transports and passengers at CLP_CARRY (who = the ferry) as upstream's ORDERS BY
--    PRIORITY does; a passenger another took is let go.
--  - The parked transports' "one on its way to a dock" is not counted (a transport with any order is left alone), so
--    the shares are the idle ones only; the log line is the port's (upstream's "# ferry:" lines).
--
-- ACROSS THE WATER (stations.c's pushes, upstream ON, measured worse than the landings - it runs only when
-- settings.push_across AND settings.expand_across are true): a push's soldiers go over first, as one wave a time
-- (ONE WAVE AT A TIME) - none while a wave of it is on its way, GATHERED FIRST for FY_GATHER at most (room for three
-- quarters of it, the first wave for all), THE TRANSPORTS HELD meanwhile (no other trip takes a free one),
-- BOARD TOGETHER (the wave sails when all of it is aboard, FY_TOGETHER at most) after THE LAST WORD (stations.lua's
-- judgement of what is aboard against the beach and the target, its losses planned for; called off: unloaded at home and
-- the push over), TOGETHER at the meeting point off the beach. The first wave takes THE BRIDGEHEAD's citizens along (2 a
-- transport, 6 at most) and puts them off for the stations' bridgehead; every soldier put off is the stations' again
-- (SN.ferried). THE SQUADRON AHEAD (squadrons.c) is not ported: the meeting point waits for the wave's transports alone.
-- A WAVE'S ORDERS (settings.push_transport_orders, off): no more than that many transports trained for one wave.
--
-- WIRING (the integrator's; this module edits nothing else): main.lua's step calls FY.step(view, memory, orders) BEFORE
-- towns.lua's (TW.step), so the service's hulls are its own before an outpost's ferry looks for one. towns.lua's
-- ex_ferry_ship takes any idle empty transport, held by a script or not, so with the service on keep [ai_base]
-- expand_islands = 0 (its default) or guard its first line with
--     if S.ferry_service == true then return end
--   and note that then nothing carries a new outpost's builders (build.c's bd_waits, a foundation waiting for them, is
-- not ported; the service carries citizens only to a land with a drop site and free places it wants, soldiers to one
-- short of its share). navy.lua keeps its landings' transports taken, so the service never touches a landing's.
local CX = require("ageofai.crossing")
local CL = require("ageofai.claims")
local TV = require("ageofai.territories")
local SN = require("ageofai.stations")   -- [stations.c] the pushes across the water this ferry carries
local S = require("ageofai.settings")
local M = {}

-- ferry.c's defines (integers: the engine validates the script's memory and takes no floats)
local FY_LOOK = 5000         -- FY_LOOK: the look
local FY_SHORE = 60          -- FY_SHORE: shores searched within this many tiles (a trip's pick-up and drop)
local FY_BOARD = 240000      -- FY_BOARD: no one aboard this long: given up
local FY_LOAD = 90000        -- FY_LOAD: some aboard this long: off with those
local FY_SEA = 360000        -- FY_SEA: not unloaded this long: put off where it is
local FY_APPROACH = 120000   -- FY_APPROACH: the transport not at the pick-up this long: the load given where it is
local FY_BOARD_AFTER = FY_BOARD - FY_APPROACH
local FY_WAY_DETOUR = 150    -- FY_WAY_DETOUR 1.5 (per cent)
local FY_WAY_SLACK = 60000   -- FY_WAY_SLACK
local FY_ORDER = 90000       -- FY_ORDER: a transport ordered counts as coming this long
local FY_CLAIM_T = 20000     -- FY_CLAIM_T: the trip's transport and passengers are its at CLP_CARRY this long
local FY_CIT_MIN = 4         -- FY_CIT_MIN: a citizens' trip takes this many at least (thrift)
local FY_PARK = 8            -- FY_PARK: a parked transport this far off its dock at most
local FY_FREE_MAX = 4        -- FY_FREE_MAX: so many free, none trained where the game puts it
local FY_HOLD_MAX = 480000   -- FY_HOLD_MAX: a trip held at the pick-up for the way waits this long at most
local FY_WAY_T = 180000      -- FY_WAY_T: a way held for the risk is worth the fleet's while this long
local FY_TRIPS = 4           -- FY_TRIPS 16 upstream: the trips on at once (the port's memory and step budget)
local FY_PASS_MAX = 16       -- a trip's passengers (upstream's ee_unit *pass[16])
local FY_GATHER = 180000     -- FY_GATHER: a push waits this long at most for transports enough
local FY_MEET = 75000        -- FY_MEET: at the meeting point this long at most for the push's other transports
local FY_MEET_SAIL = 150000  -- ... a transport at sea this long counts as there
local FY_TOGETHER = 90000    -- FY_TOGETHER: a push's loaded transport waits this long at most for the wave's others
local FY_BH_TRIP = 2         -- THE BRIDGEHEAD: a push's citizens a transport of its first wave takes at most
local SN_CITS = 6            -- ... and a first wave in all (stations.c SN_CITS)

local DROPS = {"b_town_center", "b_capitol", "b_settlement"}
local MINERALS = {stone = true, gold = true, iron = true}

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

-- the class data, cached for the game (static)
local classes = {}
local function cls(t)
    if t == nil then
        return {}
    end
    local c = classes[t]
    if c == nil then
        c = ai.class(t) or {}
        classes[t] = c
    end
    return c
end

-- the ferry's say: one line a minute at most (upstream's "# ferry:")
local function log(f, view, key, text)
    f.said = f.said or {}
    if view.time - (f.said[key] or -100000) >= 60000 then
        f.said[key] = view.time
        print(string.format("ferry P%d at %.1f min: %s", view.player or 0, view.time / 60000.0, text))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The state (memory.ferry): the trips, the look's counters, the ways waited for the fleet, the lands' tables

local function new_trip()
    return {phase = 0, no = 0, kind = 0, imp = false, ship = 0, stype = "", npass = 0, pass = {},
            from = 0, to = 0, ocean = 0, pick_x = -1, pick_y = -1, pick_wx = -1, pick_wy = -1,
            drop_x = -1, drop_y = -1, drop_wx = -1, drop_wy = -1, dest_x = -1, dest_y = -1,
            app_ms = FY_APPROACH, sea_ms = FY_SEA, since = 0, load_t = 0, there_t = 0, reissue = 0,
            rate_t = 0, risk = 0, via_x = 0, via_y = 0, has_via = false, via_done = false, held_t = 0,
            meet_t = 0, ready_t = 0, push = -1, nsold = 0, mv_t = 0, unl_t = 0, x = -1, y = -1}
end

local function state(memory)
    local f = memory.ferry
    if f == nil then
        f = {look_t = 0, trips = {}, n = 0, last_kind = 0, risky = {}, order_t = {}, lands = nil, order = {},
             lands_t = 0, asks = {}, want_t = 0, trips_n = 0, soldiers = 0, citizens = 0, failed = 0, sunk = 0,
             trained = 0, no_ship = 0, no_shore = 0, no_dock = 0, small = 0, parked = 0, held = 0, held_load = 0,
             gone_high = 0, waited = 0, sailed = 0, way = nil, way_t = 0, log_t = 0, said = {}, park_t = 0,
             wave_t = {}, pord = {}, waves = 0, pushed = 0, off = 0, transit = {}}
        for k = 1, FY_TRIPS do
            f.trips[k] = new_trip()
        end
        for k = 1, 4 do
            f.order_t[k] = 0
        end
        memory.ferry = f
    end
    return f
end

-- the service's switch: [ai_base] ferry_service (the task's default: off until the integrative wiring is agreed)
function M.on()
    return S.ferry_service == true
end

-- a new game or a load: the trips, the counters and crossing.lua's sunk places are gone
function M.new_game(memory)
    memory.ferry = nil
    CX.new_game(memory)
end

-- the ways a trip was held for (the port's stand-in for the squadrons' value of a sea: squadrons.c is not ported)
function M.ways(memory)
    local f = memory.ferry
    if f == nil or f.way == nil or (f.way_t or 0) == 0 then
        return {}
    end
    return {f.way}
end

-- the free transports: its own, a ship, on the map, empty, nobody else's (fy_free_ship: no landing's, no outpost
-- ferry's, no rescue's, no trip's - the engine's held = "engine" is every manager's released, ours are "lua")
local function free_transports(view, memory, f)
    local out = {}
    local ids = ai.units{category = "transport", held = "engine", limit = 32}
    if #ids == 0 then
        return out
    end
    local cols = ai.get(ids, {"x", "y", "inside", "cargo", "type", "hp", "speed"})
    for k = 1, #ids do
        local t = cols.type[k]
        local cap = t ~= nil and (cls(t).transport or 0) or 0
        if cols.x[k] ~= nil and cols.inside[k] == nil and (cols.cargo[k] or 0) <= 0 and cap > 0 then
            out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], type = t, cap = cap,
                             hp = cols.hp[k] or 0, speed = cols.speed[k] or 0}
        end
    end
    return out
end

-- the sea a ship stands on (0: none)
local function sea_of(x, y)
    local t = ai.tile(x, y)
    if t ~= nil and (t.sea or 0) ~= 0 then
        return t.sea
    end
    local _, _, oc = ai.shore(x, y, {max = 2})
    return oc or 0
end

-- the shore tile of a land on a sea nearest (x, y), and the water beside it: x, y, wx, wy (nil: none)
local function shore_of(land, oc, x, y)
    local sx, sy = ai.shore(x, y, {sea = oc, land = land, max = FY_SHORE})
    if sx == nil then   -- THE SHORE ANYWHERE: the whole land (upstream stops at the land's box; here 250 tiles)
        sx, sy = ai.shore(x, y, {sea = oc, land = land, max = 250})
    end
    if sx == nil then
        return nil
    end
    local wx, wy = CX.water_near(sx, sy, oc, 2)
    if wx == nil then
        return nil
    end
    return sx, sy, wx, wy
end

-- the newest transport the docks can make
local function transport_type(view)
    local best = nil
    for _, c in ipairs(view.costs) do
        if not c.building and c.producer == "b_dock" then
            local k = cls(c.type)
            if (k.transport or 0) > 0 and (best == nil or c.type > best) then
                best = c.type
            end
        end
    end
    return best
end

-- ---------------------------------------------------------------------------------------------------------------
-- The lands (the stations' lands and work.c's tables are not ported: the port builds its own from its units)

local function land_of(lands, order, cont)
    local l = lands[cont]
    if l == nil then
        l = {cont = cont, cits = 0, idle = 0, wood = 0, sold = 0, spare = 0, free = 0, wfree = 0, gap = 0,
             drop_x = -1, drop_y = -1, want = 0, want_x = -1, want_y = -1, best_want = 0, hot = false}
        lands[cont] = l
        order[#order + 1] = cont
    end
    return l
end

-- the lands and their free places, at most a look old: {lands by continent, order (deterministic)}, in f. (Upstream's
-- FY_FRESH gate is for the stations' and work.c's tables; the port's own are rebuilt at every look.)
local function lands_fresh(view, memory, f, now)
    if f.lands ~= nil and f.lands_t > 0 and now >= f.lands_t and now - f.lands_t < FY_LOOK then
        return f.lands, f.order
    end
    local lands, order = {}, {}
    f.lands, f.order, f.lands_t = lands, order, (now > 0) and now or 1
    -- its own drop sites: the lands a trip can put citizens off at
    for _, t in ipairs(DROPS) do
        for _, id in ipairs(ai.units{type = t, built = true}) do
            local u = ai.unit(id)
            if u ~= nil then
                local tile = ai.tile(u.x, u.y)
                local cont = tile ~= nil and (tile.continent or 0) or 0
                if cont ~= 0 then
                    local l = land_of(lands, order, cont)
                    if l.drop_x < 0 then
                        l.drop_x, l.drop_y = u.x, u.y
                    end
                end
            end
        end
    end
    -- the passengers already the ferry's count for the land they go to, not for the one they stand on
    local aboard = {}
    for _, t in ipairs(f.trips) do
        if t.phase ~= 0 then
            for _, id in ipairs(t.pass) do
                aboard[id] = true
            end
        end
    end
    -- the free soldiers (no manager holds them: the force's are army.lua's) and the citizens' idle and wood
    local ids = ai.units{category = "soldier", held = "engine", limit = 200}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "inside"})
        for k, id in ipairs(ids) do
            if cols.x[k] ~= nil and not aboard[id] then
                local tile = ai.tile(cols.x[k], cols.y[k])
                local cont = tile ~= nil and (tile.continent or 0) or 0
                if cont ~= 0 then
                    local l = land_of(lands, order, cont)
                    l.sold = l.sold + 1
                end
            end
        end
    end
    ids = ai.units{category = "citizen", held = "engine", limit = 200}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "inside", "action", "gathers"})
        for k, id in ipairs(ids) do
            if cols.x[k] ~= nil and not aboard[id] then
                local tile = ai.tile(cols.x[k], cols.y[k])
                local cont = tile ~= nil and (tile.continent or 0) or 0
                if cont ~= 0 then
                    local l = land_of(lands, order, cont)
                    l.cits = l.cits + 1
                    if cols.action[k] == "idle" then
                        l.idle = l.idle + 1
                    elseif cols.action[k] == "gather" and cols.gathers[k] == "wood" then
                        l.wood = l.wood + 1
                    end
                end
            end
        end
    end
    -- the free places at the mines it wants more citizens on, and at the trees of a land with idle ones (work.c)
    for _, s in ipairs(view.sites) do
        if s.has_drop and (s.room or 0) > 0 and s.blocked == nil and s.resource ~= nil and s.x ~= nil then
            local tile = ai.tile(s.x, s.y)
            local cont = tile ~= nil and (tile.continent or 0) or 0
            if cont ~= 0 then
                local l = land_of(lands, order, cont)
                if MINERALS[s.resource] then
                    if (view.wanted[s.resource] or 0) > (view.gatherers[s.resource] or 0) then
                        l.free = l.free + s.room
                    end
                elseif s.resource == "wood" and l.idle > 0 then
                    l.wfree = l.wfree + s.room
                end
            end
        end
    end
    -- the lands' shares (territories.lua's guards) and which is hot (a reinforcement to it is important)
    for i = 1, TV.count() do
        local info = TV.info(i)
        if info ~= nil and info.land and (info.cont or 0) ~= 0 then
            local l = land_of(lands, order, info.cont)
            local w = info.want or 0
            l.want = l.want + w
            if w > l.best_want then
                l.best_want = w
                l.want_x, l.want_y = info.x, info.y
            end
            if (info.state or 0) >= TV.TV_UNSAFE or (info.foe or 0) > 0 then
                l.hot = true
            end
        end
    end
    for _, cont in ipairs(order) do
        local l = lands[cont]
        l.spare = l.idle + l.wood
        if l.want_x < 0 and l.drop_x >= 0 then
            l.want_x, l.want_y = l.drop_x, l.drop_y
        end
        l.gap = l.sold - l.want
    end
    -- those aboard or on their way count for the land they go to (fy_look's sgap / cfree / cwood)
    for _, t in ipairs(f.trips) do
        if t.phase ~= 0 and lands[t.to] ~= nil then
            local lb = lands[t.to]
            if t.kind == 1 then
                lb.gap = lb.gap + t.npass
            elseif t.kind == 2 then
                local m = math.min(t.npass, math.max(lb.free, 0))
                lb.free = lb.free - m
                lb.wfree = math.max(lb.wfree - (t.npass - m), 0)
            end
        end
    end
    return lands, order
end

-- the pair that moves most (fy_look): the short land and the one with spare; the citizens' destination with the most
-- free places and the land with the most to spare (THRIFT: its own free places first, FY_CIT_MIN at least)
local function pick_pairs(lands, order, f)
    local thrift = S.ferry_thrift == true
    local bs, as_, bn = nil, nil, 0
    for _, b in ipairs(order) do
        local lb = lands[b]
        if lb.gap < 0 then
            for _, a in ipairs(order) do
                local la = lands[a]
                if a ~= b and la.gap > 0 then
                    local n = math.min(-lb.gap, la.gap)
                    if n > bn then
                        bn, bs, as_ = n, b, a
                    end
                end
            end
        end
    end
    local bc, ac, cn = nil, nil, 0
    for _, b in ipairs(order) do
        local lb = lands[b]
        local places = math.max(lb.free, 0) + math.max(lb.wfree, 0)
        if places >= 1 and lb.drop_x >= 0 then
            local have = bc ~= nil and (math.max(lands[bc].free, 0) + math.max(lands[bc].wfree, 0)) or 0
            if bc == nil or places > have then
                bc = b
            end
        end
    end
    if bc ~= nil then
        local lb = lands[bc]
        for _, a in ipairs(order) do
            local la = lands[a]
            if a ~= bc and la.spare >= 1 then
                local sp = la.spare
                if thrift and la.free > 0 then
                    sp = sp - la.free
                end
                if sp < 0 then
                    sp = 0
                end
                local si = la.idle
                if thrift and la.idle > sp then
                    si = sp
                end
                local mines = lb.free > 0 and math.min(lb.free, sp) or 0
                local wood = (lb.wfree > 0 and si > mines) and math.min(lb.wfree, si - mines) or 0
                local n = mines + wood
                if n > cn or (ac ~= nil and n == cn and la.spare > lands[ac].spare) then
                    cn, ac = n, a
                end
            end
        end
        if thrift and ac ~= nil and cn < FY_CIT_MIN and cn < (lb.free + lb.wfree) then
            f.small = f.small + 1
            ac = nil
        end
    end
    return bs, as_, bn, bc, ac, cn
end

-- ---------------------------------------------------------------------------------------------------------------
-- The trips

local function free_trip_slot(f)
    for k = 1, FY_TRIPS do
        if f.trips[k].phase == 0 then
            return f.trips[k]
        end
    end
    return nil
end

-- whether a trip of the ferry has the unit of this id (fy_carries_id)
local function carries(f, id)
    for _, t in ipairs(f.trips) do
        for _, p in ipairs(t.pass) do
            if p == id then
                return true
            end
        end
    end
    return false
end

-- the way's time at the transport's speed: the tiles x FY_WAY_DETOUR at its centi-tiles a second, FY_WAY_SLACK, at
-- least lo, at most an hour (fy_way_ms)
local function way_ms(speed, tiles, lo)
    local v = speed or 0
    if v < 20 then
        v = 20
    end
    local ms = tiles * FY_WAY_DETOUR * 1000 // v + FY_WAY_SLACK
    if ms > 3600000 then
        ms = 3600000
    end
    return ms > lo and ms or lo
end

-- one trip's end: someone aboard is put off at the nearest land with room, the transport and passengers are the
-- others' again (fy_end)
local function end_trip(view, memory, f, t, now, why)
    local ship = ai.unit(t.ship)
    if ship ~= nil and t.phase >= 2 and (ship.cargo or 0) > 0 then
        local sx, sy = ai.shore(ship.x, ship.y, {sea = t.ocean, land = t.to, max = 40})
        if sx == nil then
            sx, sy = ai.shore(ship.x, ship.y, {sea = t.ocean, land = t.from, max = 40})
        end
        if sx ~= nil then
            ai.unload(t.ship, sx, sy)
        end
    end
    if why ~= nil then
        print(string.format("ferry P%d at %.1f min: trip %d (risk %d): %s", view.player or 0, view.time / 60000.0,
            t.no, t.risk, why))
    end
    for _, id in ipairs(t.pass) do
        CL.end_(memory, id, CL.WHO.ferry)
        if ai.unit(id) ~= nil then
            ai.release(id)
        end
    end
    if t.ship ~= 0 then
        CL.end_(memory, t.ship, CL.WHO.ferry)
        if ship ~= nil then
            ai.release(t.ship)
        end
    end
    t.phase, t.npass, t.pass, t.ship = 0, 0, {}, 0
end

-- the citizens a trip may take from land a nearest (dx, dy): the idle ones first, then the wood cutters (work.c's own
-- order; the bridgehead's citizens too, as many as the trip wants)
local function pick_cits(view, memory, a, dx, dy, n)
    local ids = ai.units{category = "citizen", held = "engine", near = {dx, dy, 2000}, limit = 32}
    local cand = {}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "inside", "action", "gathers"})
        for k, id in ipairs(ids) do
            if cols.x[k] ~= nil and cols.inside[k] == nil then
                local pref = nil
                if cols.action[k] == "idle" then
                    pref = 0
                elseif cols.action[k] == "gather" and cols.gathers[k] == "wood" then
                    pref = 1
                end
                if pref ~= nil then
                    local tile = ai.tile(cols.x[k], cols.y[k])
                    if tile ~= nil and tile.continent == a
                            and CL.may(view, memory, id, CL.PRIO.carry, CL.WHO.ferry) then
                        cand[#cand + 1] = {id = id, p = pref, d = d2(cols.x[k], cols.y[k], dx, dy)}
                    end
                end
            end
        end
    end
    table.sort(cand, function(x, y)
        if x.p ~= y.p then
            return x.p < y.p
        end
        if x.d ~= y.d then
            return x.d < y.d
        end
        return x.id < y.id
    end)
    local out = {}
    for k = 1, math.min(n, #cand, FY_PASS_MAX) do
        out[k] = cand[k].id
    end
    return out
end

-- a trip of kind (1 soldiers, 2 citizens, 3 a push's soldiers, 4 scouts) of n from land a to land b, wanted at (dx, dy),
-- as trip t: 1 when it set out, 0 nobody to take, -1 no free transport on a sea between the two, -2 not now for the risk
local function start(view, memory, f, kind, imp, a, b, n, dx, dy, now, orders, t, one, push)
    -- the passengers: soldiers (the nearest the destination; the free ones - no manager holds them), citizens (the idle
    -- ones first, then the wood cutters), a scout, a push's soldiers (stations.lua's) with its bridgehead's citizens
    local pass = {}
    local cits = 0
    if kind == 1 then
        local ids = ai.units{category = "soldier", held = "engine", near = {dx, dy, 2000}, limit = 32}
        if #ids > 0 then
            local cols = ai.get(ids, {"x", "y", "inside", "action"})
            local want = math.min(n, FY_PASS_MAX)
            for k, id in ipairs(ids) do
                if #pass >= want then
                    break
                end
                if cols.x[k] ~= nil and cols.inside[k] == nil and cols.action[k] ~= "attack" then
                    local tile = ai.tile(cols.x[k], cols.y[k])
                    if tile ~= nil and tile.continent == a
                            and CL.may(view, memory, id, CL.PRIO.carry, CL.WHO.ferry) then
                        pass[#pass + 1] = id
                    end
                end
            end
        end
    elseif kind == 2 then
        pass = pick_cits(view, memory, a, dx, dy, n)
    elseif kind == 3 then
        pass = SN.push_pick(view, memory, push, dx, dy, n)
        -- THE BRIDGEHEAD: its first wave's citizens first, FY_BH_TRIP a transport
        local want = SN.bh_want(memory, push)
        local list = want > 0 and pick_cits(view, memory, a, dx, dy, math.min(want, FY_BH_TRIP)) or {}
        if #list > 0 then
            while #pass > FY_PASS_MAX - #list do
                pass[#pass] = nil
            end
            local merged = list
            for _, id in ipairs(pass) do
                merged[#merged + 1] = id
            end
            pass, cits = merged, #list
        end
    else
        if one ~= nil and ai.unit(one) ~= nil then
            pass[1] = one
        end
    end
    if #pass == 0 then
        return 0
    end
    local sx, sy, np = 0, 0, 0
    for _, id in ipairs(pass) do
        local u = ai.unit(id)
        if u ~= nil then
            sx, sy, np = sx + u.x, sy + u.y, np + 1
        end
    end
    if np == 0 then
        return 0
    end
    local mid_x, mid_y = sx // np, sy // np
    -- the transport: the nearest free one on a sea both lands touch (each sea's shores looked for once)
    local best, bd, bsea, bslot = nil, nil, nil, nil
    local seas, ns, frees = {}, 0, 0
    for _, tr in ipairs(free_transports(view, memory, f)) do
        frees = frees + 1
        local oc = sea_of(tr.x, tr.y)
        if oc ~= 0 then
            local s = nil
            for k = 1, ns do
                if seas[k].oc == oc then
                    s = seas[k]
                end
            end
            if s == nil then
                if ns < 4 then
                    s = {oc = oc, ok = false}
                    s.pick_x, s.pick_y, s.pick_wx, s.pick_wy = shore_of(a, oc, mid_x, mid_y)
                    if s.pick_x ~= nil then
                        s.drop_x, s.drop_y, s.drop_wx, s.drop_wy = shore_of(b, oc, dx, dy)
                    end
                    s.ok = s.pick_x ~= nil and s.drop_x ~= nil
                    ns = ns + 1
                    seas[ns] = s
                end
            end
            if s ~= nil and s.ok then
                local d = d2(tr.x, tr.y, s.pick_wx, s.pick_wy)
                if bd == nil or d < bd then
                    bd, best, bsea, bslot = d, tr, oc, s
                end
            end
        end
    end
    if best == nil then
        if frees > 0 then
            f.no_shore = f.no_shore + 1
        else
            f.no_ship = f.no_ship + 1
        end
        return -1
    end
    -- as many as fit (a passenger takes one place: tw_size has no field in ai.class here)
    local keep, room = {}, 0
    for _, id in ipairs(pass) do
        if room + 1 <= best.cap then
            keep[#keep + 1] = id
            room = room + 1
        end
    end
    if #keep == 0 then
        return 0
    end
    pass = keep
    -- THE CROSSING: rated before anyone boards; too risky, it waits off the shore empty; into danger with a full load
    local risk, vx, vy, hv, secs = 0, 0, 0, false, 0
    if S.ferry_safety == true then
        risk, vx, vy, hv, secs = CX.risk(view, memory, bslot.pick_wx, bslot.pick_wy, bslot.drop_wx, bslot.drop_wy,
            kind, {type = best.type, speed = best.speed, hp = best.hp, ocean = bsea})
        if risk > CX.tolerance(f.risky, kind, imp, now) then
            if (f.risky[kind] or 0) == 0 then
                f.risky[kind] = (now > 0) and now or 1
            end
            f.held = f.held + 1
            if S.sea_first == true then
                f.way = {oc = bsea, a_x = bslot.pick_wx, a_y = bslot.pick_wy, b_x = bslot.drop_wx,
                         b_y = bslot.drop_wy, via_x = vx, via_y = vy, via = hv, risk = risk,
                         t = (now > 0) and now or 1}
                f.way_t = (now > 0) and now or 1
            end
            return -2
        end
        if not imp and risk > CX.FULL and room * 2 < best.cap then
            f.held_load = f.held_load + 1
            return -2
        end
        if imp and risk > CX.GO_LOW then
            f.gone_high = f.gone_high + 1
        end
    else
        hv = false
    end
    f.risky[kind] = 0
    -- the trip
    t.phase, t.kind, t.imp, t.npass, t.pass = 1, kind, imp, #pass, pass
    t.push, t.nsold = push or -1, #pass - cits
    t.ship, t.stype, t.from, t.to, t.ocean = best.id, best.type, a, b, bsea
    t.pick_x, t.pick_y, t.pick_wx, t.pick_wy = bslot.pick_x, bslot.pick_y, bslot.pick_wx, bslot.pick_wy
    t.drop_x, t.drop_y, t.drop_wx, t.drop_wy = bslot.drop_x, bslot.drop_y, bslot.drop_wx, bslot.drop_wy
    t.dest_x, t.dest_y = dx, dy
    t.risk, t.has_via, t.via_done = risk, hv, not hv
    t.via_x, t.via_y = vx, vy
    t.app_ms, t.sea_ms = FY_APPROACH, FY_SEA
    if S.ferry_time_by_way == true then
        t.app_ms = way_ms(best.speed, isqrt(d2(best.x, best.y, t.pick_wx, t.pick_wy)), FY_APPROACH)
        local across = isqrt(d2(t.pick_wx, t.pick_wy, t.drop_wx, t.drop_wy))
        if hv then
            across = isqrt(d2(t.pick_wx, t.pick_wy, vx, vy)) + isqrt(d2(vx, vy, t.drop_wx, t.drop_wy))
        end
        t.sea_ms = way_ms(best.speed, across, FY_SEA)
    end
    -- TOGETHER: the trip meets an earlier one's way off the line - a reinforcement to a hot land (crossing.c cx_meet),
    -- and a push's wave at its meeting point off the beach (one for all its transports)
    if (kind == 1 and imp) or kind == 3 then
        local same = kind == 1 and imp
        for _, o in ipairs(f.trips) do
            if o ~= t and o.phase ~= 0 and o.has_via
                    and (same and o.kind == 1 and o.imp and o.to == b or kind == 3 and o.kind == 3 and o.push == t.push) then
                t.via_x, t.via_y, t.has_via, t.via_done = o.via_x, o.via_y, true, false
                break
            end
        end
        if not t.has_via then
            local mx, my = CX.meet(view, memory, t.pick_wx, t.pick_wy, t.drop_wx, t.drop_wy, bsea, best.type)
            if mx ~= nil then
                t.via_x, t.via_y, t.has_via, t.via_done = mx, my, true, false
            end
        end
        if kind == 1 then
            t.meet_t = t.has_via and (now > 0) and now or 0
        end
    end
    t.since, t.load_t, t.there_t, t.reissue, t.rate_t, t.held_t, t.mv_t, t.unl_t = (now > 0) and now or 1, 0, 0, 0,
        (now > 0) and now or 1, 0, 0, 0
    f.trips_n = f.trips_n + 1
    f.last_kind = kind
    t.no = f.trips_n
    f.n = f.trips_n
    -- ORDERS BY PRIORITY: the trip's transport and passengers are its at CLP_CARRY till it ends or 20 s pass
    CL.take(view, memory, best.id, CL.PRIO.carry, CL.WHO.ferry, FY_CLAIM_T)
    ai.take(best.id)
    ai.move(best.id, t.pick_wx, t.pick_wy, {forced = true})
    for _, id in ipairs(pass) do
        CL.take(view, memory, id, CL.PRIO.carry, CL.WHO.ferry, FY_CLAIM_T)
        ai.take(id)
    end
    ai.move(pass, t.pick_x, t.pick_y)   -- to the shore meanwhile
    log(f, view, "start", string.format("%d %s from land %d to land %d - the transport at %d,%d, boarding at %d,%d, off at %d,%d; risk %d",
        #pass, kind == 1 and "soldiers" or "citizens", a, b, best.x, best.y, t.pick_x, t.pick_y, t.drop_x, t.drop_y, risk))
    return 1
end

-- THE LAST WORD (stations.c sn_last_word): a push's first wave aboard, its soldiers - judged at once against the beach and
-- the target, its losses planned for; called off: every transport of the wave unloads where it boarded, the push over:
-- 1 go, 0 wait, -1 called off
local function last_word(view, memory, f, t, now)
    local ids = {}
    for _, o in ipairs(f.trips) do
        if o.phase == 2 and o.kind == 3 and o.push == t.push then
            local cols = ai.get(o.pass, {"inside", "category"})
            for k, id in ipairs(o.pass) do
                if cols.inside[k] ~= nil and cols.category[k] ~= "citizen" then
                    ids[#ids + 1] = id
                end
            end
        end
    end
    local r = SN.last_word(view, memory, t.push, ids, now)
    if r >= 0 then
        return r
    end
    for _, o in ipairs(f.trips) do
        if o.phase ~= 0 and o.phase < 3 and o.kind == 3 and o.push == t.push then
            local ship = ai.unit(o.ship)
            if ship ~= nil and (ship.cargo or 0) > 0 then
                ai.unload(o.ship, o.pick_x, o.pick_y)
            end
            end_trip(view, memory, f, o, now, "called off - a beach too strong, unloaded at home")
        end
    end
    SN.push_off(memory, t.push, now)
    f.off = (f.off or 0) + 1
    return -1
end

-- one trip's step (fy_trip_act)
local function trip_act(view, memory, f, t, now)
    local ship = ai.unit(t.ship)
    if ship == nil then
        f.sunk, f.failed = f.sunk + 1, f.failed + 1
        if t.phase == 3 then
            CX.sunk(memory, t.x, t.y, now)   -- THE CROSSING: as if what sank it were there still
        end
        end_trip(view, memory, f, t, now, t.phase == 3 and "the transport was sunk on the way" or "the transport is gone")
        return
    end
    t.x, t.y = ship.x, ship.y
    local aboard, keep = 0, {}
    if t.npass > 0 then
        local cols = ai.get(t.pass, {"x", "y", "inside"})
        for k, id in ipairs(t.pass) do
            if cols.inside[k] ~= nil then
                keep[#keep + 1] = id
                aboard = aboard + 1
            elseif cols.x[k] ~= nil and CL.take(view, memory, id, CL.PRIO.carry, CL.WHO.ferry, FY_CLAIM_T) then
                keep[#keep + 1] = id
            end
        end
        t.pass, t.npass = keep, #keep
    end
    if not CL.take(view, memory, t.ship, CL.PRIO.carry, CL.WHO.ferry, FY_CLAIM_T) then
        return   -- out of a storm or a danger zone first: the trip's orders wait
    end
    if t.phase == 1 or t.phase == 2 then
        if t.npass == 0 then
            end_trip(view, memory, f, t, now, "no passenger left")
            return
        end
        local there = d2(ship.x, ship.y, t.pick_wx, t.pick_wy) <= 49
        if there and t.there_t == 0 then
            t.there_t = (now > 0) and now or 1
        end
        local waits = t.phase == 1 and t.held_t > 0 and t.load_t == 0 and aboard == 0
        if waits and now - t.held_t >= FY_HOLD_MAX then
            f.failed = f.failed + 1
            end_trip(view, memory, f, t, now, "held at the pick-up for the risk - given up")
            return
        end
        if not waits and now - t.since >= t.app_ms + FY_BOARD_AFTER and aboard == 0 then
            f.failed = f.failed + 1
            end_trip(view, memory, f, t, now, "boarding failed in time")
            return
        end
        if t.phase == 1 then
            if not there and now - t.since < t.app_ms then
                if now - t.mv_t >= 5000 then
                    t.mv_t = now
                    ai.move(t.ship, t.pick_wx, t.pick_wy, {forced = true})
                end
                return
            end
            if S.ferry_safety == true and aboard == 0 then
                -- THE CROSSING: rated again before anyone boards - too risky, it waits off the shore, empty
                if now - t.rate_t >= CX.LOOK then
                    t.rate_t = now
                    local risk, vx, vy, hv = CX.risk(view, memory, t.pick_wx, t.pick_wy, t.drop_wx, t.drop_wy, t.kind,
                        {type = t.stype, speed = ship.speed or 0, hp = ship.hp or 0, ocean = t.ocean})
                    t.risk = risk
                    if t.kind ~= 1 or not t.imp then
                        t.via_x, t.via_y, t.has_via, t.via_done = vx, vy, hv, not hv
                    end   -- a hot land's reinforcement keeps its meeting point
                    if risk > CX.tolerance(f.risky, t.kind, t.imp, now) then
                        if (f.risky[t.kind] or 0) == 0 then
                            f.risky[t.kind] = (now > 0) and now or 1
                        end
                        t.load_t = 0
                        if S.sea_first == true then
                            f.way = {oc = t.ocean, a_x = t.pick_wx, a_y = t.pick_wy, b_x = t.drop_wx, b_y = t.drop_wy,
                                     via_x = t.via_x, via_y = t.via_y, via = t.has_via, risk = risk,
                                     t = (now > 0) and now or 1}
                            f.way_t = (now > 0) and now or 1
                            if t.held_t == 0 then
                                t.held_t = (now > 0) and now or 1
                                f.waited = f.waited + 1
                            end
                        end
                    else
                        t.load_t = 1
                    end
                end
                if t.load_t == 0 then
                    if now - t.mv_t >= 5000 then
                        t.mv_t = now
                        ai.move(t.ship, t.pick_wx, t.pick_wy, {forced = true})
                    end
                    return
                end
            end
            if t.held_t > 0 then
                f.sailed = f.sailed + 1
                t.held_t = 0
            end
            t.phase = 2
            t.load_t = (now > 0) and now or 1
        end
        -- boarding: all aboard, or some aboard for FY_LOAD: off with those
        local ready = (t.npass > 0 and aboard == t.npass) or (aboard > 0 and now - t.load_t >= FY_LOAD)
        if ready and t.kind == 3 then
            -- BOARD TOGETHER (as a landing's): a push's wave sails when all of it is aboard, FY_TOGETHER at most
            if t.ready_t == 0 then
                t.ready_t = (now > 0) and now or 1
            end
            local wait, first = false, t.ready_t
            for _, o in ipairs(f.trips) do
                if o ~= t and o.phase ~= 0 and o.phase < 3 and o.kind == 3 and o.push == t.push then
                    if o.ready_t == 0 then
                        wait = true
                    elseif o.ready_t < first then
                        first = o.ready_t
                    end
                end
            end
            if wait and now - first < FY_TOGETHER then
                ready = false
            end
            if ready then   -- THE LAST WORD: judged against the beach and the target, its losses planned for
                local r = last_word(view, memory, f, t, now)
                if r < 0 then
                    return
                end
                if r == 0 then
                    ready = false
                end
            end
        end
        if ready then
            local left = {}
            local cols = ai.get(t.pass, {"inside"})
            for k, id in ipairs(t.pass) do
                if cols.inside[k] ~= nil then
                    left[#left + 1] = id
                else   -- those left behind are the others' again
                    CL.end_(memory, id, CL.WHO.ferry)
                    if ai.unit(id) ~= nil then
                        ai.release(id)
                    end
                end
            end
            t.pass, t.npass = left, #left
            if t.has_via then
                ai.move(t.ship, t.via_x, t.via_y, {forced = true})
            else
                ai.unload(t.ship, t.drop_x, t.drop_y)
            end
            t.phase, t.since, t.mv_t = 3, now, now
            return
        end
        -- the load goal, again every 20 s for those not aboard
        if t.reissue ~= 0 and now - t.reissue < 20000 then
            return
        end
        t.reissue = (now > 0) and now or 1
        local list = {}
        local cols = ai.get(t.pass, {"x", "inside"})
        for k, id in ipairs(t.pass) do
            if cols.inside[k] == nil and cols.x[k] ~= nil then
                list[#list + 1] = id
            end
        end
        if #list > 0 then
            ai.load(list, t.ship)
        end
        return
    end
    -- phase 3: across
    if (ship.cargo or 0) <= 0 then
        local cols = ai.get(t.pass, {"x", "inside", "category"})
        local off, offc = 0, 0
        for k, id in ipairs(t.pass) do
            if cols.inside[k] == nil and cols.x[k] ~= nil then
                off = off + 1
                if cols.category[k] == "citizen" then
                    offc = offc + 1
                end
            end
        end
        if t.kind == 2 then
            f.citizens = f.citizens + off
        else
            f.soldiers = f.soldiers + off - offc
            f.citizens = f.citizens + offc
        end
        if t.kind ~= 2 then   -- on to where they are wanted (a citizen's work is work.c's)
            for k, id in ipairs(t.pass) do
                if cols.inside[k] == nil and cols.x[k] ~= nil then
                    if t.kind == 3 and cols.category[k] == "citizen" then
                        SN.bh_land(memory, t.push, id)   -- THE BRIDGEHEAD: its citizens, the stations'
                        ai.move(id, t.dest_x, t.dest_y)
                    else
                        ai.move(id, t.dest_x, t.dest_y)
                        if t.kind == 3 then
                            SN.ferried(memory, id, t.from)   -- ACROSS: the push's soldier, its station again
                        end
                    end
                end
            end
        end
        print(string.format("ferry P%d at %.1f min: trip %d (risk %d): %d %s put off on land %d at %d,%d%s",
            view.player or 0, view.time / 60000.0, t.no, t.risk, off, t.kind == 2 and "citizens" or "soldiers",
            t.to, t.drop_x, t.drop_y, offc > 0 and " - the bridgehead's citizens among them" or ""))
        end_trip(view, memory, f, t, now, nil)
        return
    end
    if now - t.since >= t.sea_ms then
        f.failed = f.failed + 1
        end_trip(view, memory, f, t, now, "not across in time - put off where it is")
        return
    end
    -- TOGETHER: a push's transports meet off the beach and run in at once (THE SQUADRON AHEAD is not ported)
    if t.kind == 3 and t.has_via and not t.via_done and (d2(ship.x, ship.y, t.via_x, t.via_y) <= 36
            or now - t.since >= FY_MEET_SAIL) then
        if t.meet_t == 0 then
            t.meet_t = (now > 0) and now or 1
        end
        local all, first = true, t.meet_t
        for _, o in ipairs(f.trips) do
            if o.phase == 3 and o.kind == 3 and o.push == t.push then
                if o.meet_t == 0 then
                    all = false
                elseif o.meet_t < first then
                    first = o.meet_t
                end
            end
        end
        if all or now - first >= FY_MEET then
            for _, o in ipairs(f.trips) do
                if o.phase == 3 and o.kind == 3 and o.push == t.push and o.meet_t > 0 then
                    o.via_done = true
                end
            end
        end
        if not t.via_done then
            return
        end
    end
    if t.has_via and not t.via_done then   -- THE CROSSING: the way off the line - its point first
        if d2(ship.x, ship.y, t.via_x, t.via_y) <= 36 then
            t.via_done = true
        else
            if now - t.mv_t >= 5000 then
                t.mv_t = now
                ai.move(t.ship, t.via_x, t.via_y, {forced = true})
            end
            return
        end
    end
    if ship.action ~= "unload" and now - t.unl_t >= 4000 then
        t.unl_t = now
        ai.unload(t.ship, t.drop_x, t.drop_y)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- More transports (fy_order) and the park (fy_park)

local function order_transports(view, memory, f, orders, wanted, wt, now)
    local coming, slot = 0, 0
    for k = 1, 4 do
        local o = f.order_t[k]
        if o > 0 and now >= o and now - o < FY_ORDER then
            coming = coming + 1
        elseif slot == 0 then
            slot = k
        end
    end
    if wanted <= coming or slot == 0 then
        return
    end
    -- NOT WHERE IT CAN'T SERVE: a dock on a sea both lands of a wanted trip touch (the first trip's; upstream tries
    -- each - a shore search a dock costs here)
    local docks = {}
    for _, id in ipairs(ai.units{type = "b_dock", built = true, limit = 16}) do
        local u = ai.unit(id)
        if u ~= nil then
            local _, _, oc = ai.shore(u.x, u.y, {max = 5})
            if (oc or 0) ~= 0 then
                docks[#docks + 1] = {id = id, x = u.x, y = u.y, oc = oc}
            end
        end
    end
    local sea, dock = nil, nil
    local w = wt ~= nil and wt[1] or nil
    if w ~= nil then
        for _, d in ipairs(docks) do
            if sea == nil and ai.shore(w.from_x, w.from_y, {sea = d.oc, land = w.a, max = FY_SHORE}) ~= nil
                    and ai.shore(w.to_x, w.to_y, {sea = d.oc, land = w.b, max = FY_SHORE}) ~= nil then
                sea, dock = d.oc, d
            end
        end
    end
    if sea == nil and S.ferry_thrift == true then
        f.no_dock = f.no_dock + 1
        return
    end
    if sea == nil then
        -- water between the two lands at all? (no free one: none where the game puts it while FY_FREE_MAX stand)
        local water = false
        if w ~= nil then
            for _, e in ipairs(TV.seas()) do
                if not water and (e.ocean or 0) ~= 0
                        and ai.shore(w.from_x, w.from_y, {sea = e.ocean, land = w.a, max = FY_SHORE}) ~= nil
                        and ai.shore(w.to_x, w.to_y, {sea = e.ocean, land = w.b, max = FY_SHORE}) ~= nil then
                    water = true
                end
            end
        end
        if not water or #free_transports(view, memory, f) >= FY_FREE_MAX then
            f.no_dock = f.no_dock + 1
            return
        end
    end
    local tt = transport_type(view)
    if tt == nil then
        return
    end
    local o = {verb = "train", type = tt, n = 1}
    if dock ~= nil then
        o.building = dock.id   -- trained at the dock on the trip's sea (upstream's g_expand_econ's train)
    end
    if orders ~= nil then
        orders[#orders + 1] = o
    end
    f.order_t[slot] = (now > 0) and now or 1
    f.trained = f.trained + 1
    log(f, view, "order", string.format("trains a transport (%s): %d trips found none free, %d ordered before still to come%s",
        tt, wanted, coming, dock ~= nil and " - at a dock on the sea of the trip" or ""))
end

-- HOME TO A DOCK: a free transport of its with no order, empty and nobody's, goes to the water by its nearest
-- finished dock on its sea that is out of danger; a sea's free ones spread over its docks in even shares
local function park(view, memory, f, now)
    if now - f.park_t < FY_LOOK then
        return
    end
    f.park_t = (now > 0) and now or 1
    local dk, nd = {}, 0
    for _, id in ipairs(ai.units{type = "b_dock", built = true, limit = 32}) do
        local u = ai.unit(id)
        if u ~= nil and not TV.unsafe(u.x, u.y) and (ai.danger == nil or (ai.danger(u.x, u.y) or 0) == 0) then
            local oc = sea_of(u.x, u.y)
            if oc ~= 0 then
                local wx, wy = CX.water_near(u.x, u.y, oc, 4)
                if wx ~= nil then
                    nd = nd + 1
                    dk[nd] = {x = wx, y = wy, oc = oc, n = 0}
                end
            end
        end
    end
    if nd == 0 then
        return
    end
    -- its free transports with no order: the dock they lie by (-1 none)
    local sh, nsh = {}, 0
    for _, tr in ipairs(free_transports(view, memory, f)) do
        local u = ai.unit(tr.id)
        if u ~= nil and u.action == "idle" then
            local oc = sea_of(tr.x, tr.y)
            local dock, bd = -1, nil
            for k = 1, nd do
                if dk[k].oc == oc then
                    local d = d2(tr.x, tr.y, dk[k].x, dk[k].y)
                    if d <= FY_PARK * FY_PARK and (dock < 0 or d < bd) then
                        dock, bd = k, d
                    end
                end
            end
            if dock >= 0 then
                dk[dock].n = dk[dock].n + 1
            end
            nsh = nsh + 1
            sh[nsh] = {id = tr.id, x = tr.x, y = tr.y, oc = oc, dock = dock}
        end
    end
    local spread = S.ferry_thrift == true
    for i = 1, nsh do
        local docks, ships = 0, 0
        for k = 1, nd do
            if dk[k].oc == sh[i].oc then
                docks = docks + 1
                ships = ships + dk[k].n
            end
        end
        for j = 1, nsh do
            if sh[j].dock < 0 and sh[j].oc == sh[i].oc then
                ships = ships + 1
            end
        end
        local share = docks > 0 and (ships + docks - 1) // docks or 0   -- SPREAD: even shares, rounded up
        if not (sh[i].dock >= 0 and (not spread or dk[sh[i].dock].n <= share)) then
            local b, bd = -1, nil
            for k = 1, nd do
                if dk[k].oc == sh[i].oc and k ~= sh[i].dock
                        and (not spread or sh[i].dock < 0 or dk[k].n + 1 <= share) then
                    local d = d2(sh[i].x, sh[i].y, dk[k].x, dk[k].y) + (spread and dk[k].n * 400 or 0)
                    if b < 0 or d < bd then
                        b, bd = k, d
                    end
                end
            end
            if b >= 0 then
                if sh[i].dock >= 0 then
                    dk[sh[i].dock].n = dk[sh[i].dock].n - 1
                end
                dk[b].n = dk[b].n + 1
                sh[i].dock = b
                ai.move(sh[i].id, dk[b].x, dk[b].y, {forced = true})
                f.parked = f.parked + 1
            end
        end
    end
end

-- ACROSS (stations.c push_across, ferry.c ONE WAVE AT A TIME): a push's soldiers first - every free transport at once,
-- important ones (the transports may be lost once they are over); those taken leave the push's count of soldiers still
-- to cross. ONE WAVE AT A TIME: none while a wave of it is on its way, its free transports taking three quarters of it
-- (the first wave all of it) first - GATHERED FIRST - and THE TRANSPORTS HELD meanwhile (no other trip takes a free
-- one). Returns held (nothing else may take a transport this look), the trips that found none, and what was pushed.
local function push_waves(view, memory, f, now, orders)
    local out = {held = false, want_n = 0, wants = {}}
    if S.push_across ~= true or S.expand_across ~= true then
        return out
    end
    local pushes = SN.across(memory)
    if #pushes == 0 then
        return out
    end
    f.wave_t = f.wave_t or {}
    local frees = free_transports(view, memory, f)
    local room, cap = 0, 10
    for _, tr in ipairs(frees) do
        room = room + tr.cap
        if tr.cap > cap then
            cap = tr.cap
        end
    end
    for _, q in ipairs(pushes) do
        local k = q.k
        local transit = 0
        for _, t in ipairs(f.trips) do
            if t.phase ~= 0 and t.kind == 3 and t.push == k then
                transit = transit + t.nsold
            end
        end
        local left0 = q.left - transit
        local wave_t = f.wave_t[k] or 0
        if left0 <= 0 or transit > 0 then
            f.wave_t[k] = 0
        elseif q.waves > 0 and left0 < (cap + 1) // 2 then
            f.wave_t[k] = 0   -- a later wave: half a transport at least
        else
            if wave_t == 0 then
                wave_t = (now > 0) and now or 1
                f.wave_t[k] = wave_t
            end
            -- THE BRIDGEHEAD: 2 citizens a transport of the first wave, SN_CITS at most
            if q.waves == 0 and SN.bh_want(memory, k) == 0 then
                local cw = 2 * ((left0 + cap - 1) // cap)
                SN.bh_want_set(memory, k, cw < SN_CITS and cw or SN_CITS)
            end
            local places = #SN.push_pick(view, memory, k, q.beach_x, q.beach_y, 128)
            local need = (places > left0 and places or left0) + (q.waves > 0 and 0 or SN.bh_want(memory, k))
            -- THE WHOLE PUSH FIRST: its first wave waits for room for all of it, a later one for three quarters
            local share = q.waves > 0 and 3 or 4
            if room * 4 < need * share and now - wave_t < FY_GATHER then
                out.held = true
                local want = (need * share // 4 - room + cap - 1) // cap
                if #out.wants < 4 then
                    out.wants[#out.wants + 1] = {a = q.from_land, b = q.to_land, from_x = q.x, from_y = q.y,
                                                 to_x = q.beach_x, to_y = q.beach_y}
                end
                out.want_n = out.want_n + (want > 0 and want or 1)
            else
                local left, set, ships = left0, 0, 0
                while left > 0 do
                    local t = free_trip_slot(f)
                    if t == nil then
                        break
                    end
                    local r = start(view, memory, f, 3, true, q.from_land, q.to_land, left, q.beach_x, q.beach_y,
                        now, orders, t, nil, k)
                    if r == -1 then
                        out.want_n = out.want_n + 1
                        if #out.wants < 4 then
                            out.wants[#out.wants + 1] = {a = q.from_land, b = q.to_land, from_x = q.x, from_y = q.y,
                                                         to_x = q.beach_x, to_y = q.beach_y}
                        end
                    end
                    if r <= 0 then
                        break
                    end
                    local took = t.nsold
                    local tr = cls(t.stype)
                    room = math.max(room - (tr.transport or cap), 0)
                    left = left - took
                    set, ships = set + took, ships + 1
                end
                if ships > 0 then
                    f.waves, f.pushed = (f.waves or 0) + 1, (f.pushed or 0) + ships
                    SN.push_wave(memory, k)
                    f.wave_t[k] = 0
                    log(f, view, "wave", string.format("a push's wave %d: %d soldiers set out on %d transports",
                        q.waves + 1, set, ships))
                end
            end
        end
    end
    return out
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look (fy_look): the lands' gaps, the trips, the scouts, the transports to order

local function look(view, memory, f, orders, now)
    local lands, order = lands_fresh(view, memory, f, now)
    local unserved, wt, nwt = 0, {}, 0
    local function missed(a, b, fx, fy, tx, ty)
        unserved = unserved + 1
        if nwt < 8 then
            nwt = nwt + 1
            wt[nwt] = {a = a, b = b, from_x = fx, from_y = fy, to_x = tx, to_y = ty}
        end
    end
    local pw = push_waves(view, memory, f, now, orders)
    if pw.want_n > 0 then
        unserved = unserved + pw.want_n
        for _, w in ipairs(pw.wants) do
            if nwt < 8 then
                nwt = nwt + 1
                wt[nwt] = w
            end
        end
    end
    for _ = 1, FY_TRIPS do
        if pw.held then
            break
        end
        local t = free_trip_slot(f)
        if t == nil then
            break
        end
        local bs, as_, bn, bc, ac, cn = pick_pairs(lands, order, f)
        local can = {[1] = bs ~= nil and as_ ~= nil, [2] = bc ~= nil and ac ~= nil}
        if not can[1] and not can[2] then
            break
        end
        local first = (can[2] and (not can[1] or f.last_kind == 1)) and 2 or 1   -- TURNS
        local started = false
        for j = 0, 1 do
            local kind = j == 0 and first or 3 - first
            if can[kind] then
                local r
                if kind == 1 then
                    local la, lb = lands[as_], lands[bs]
                    r = start(view, memory, f, 1, lb.hot, as_, bs, bn, lb.want_x, lb.want_y, now, orders, t)
                else
                    local la, lb = lands[ac], lands[bc]
                    r = start(view, memory, f, 2, false, ac, bc, cn, lb.drop_x, lb.drop_y, now, orders, t)
                end
                if r > 0 then
                    if kind == 1 then   -- sgap: they are the short land's now
                        lands[bs].gap = lands[bs].gap + t.npass
                        lands[as_].gap = lands[as_].gap - t.npass
                    else
                        local lb = lands[bc]
                        local m = math.min(t.npass, math.max(lb.free, 0))
                        lb.free = lb.free - m
                        lb.wfree = math.max(lb.wfree - (t.npass - m), 0)
                        lands[ac].spare = math.max(lands[ac].spare - t.npass, 0)
                    end
                    started = true
                    break
                end
                can[kind] = false
                if r == -1 then
                    if kind == 1 then
                        missed(as_, bs, lands[as_].want_x, lands[as_].want_y, lands[bs].want_x, lands[bs].want_y)
                    else
                        missed(ac, bc, lands[ac].drop_x, lands[ac].drop_y, lands[bc].drop_x, lands[bc].drop_y)
                    end
                end
            end
        end
        if not started then
            break
        end
    end
    -- SCOUTS ACROSS: one transport a scout that asked (explore.lua's: M.scout_ask; the ask is fresh 10 s)
    for k = 1, #f.asks do
        local ask = f.asks[k]
        if ask.id ~= 0 then
            if now - ask.t > 10000 then
                ask.id = 0
            else
                local u = ai.unit(ask.id)
                if u ~= nil and u.inside == nil and not carries(f, ask.id) then
                    local from = ai.tile(u.x, u.y)
                    local to = ai.tile(ask.x, ask.y)
                    local a = from ~= nil and (from.continent or 0) or 0
                    local b = to ~= nil and (to.continent or 0) or 0
                    if a ~= 0 and b ~= 0 and a ~= b then
                        local t = free_trip_slot(f)
                        if t ~= nil then
                            local r = start(view, memory, f, 4, false, a, b, 1, ask.x, ask.y, now, orders, t, ask.id)
                            if r > 0 then
                                ask.id = 0
                            elseif r == -1 then
                                missed(a, b, u.x, u.y, ask.x, ask.y)
                            end
                        end
                    end
                end
            end
        end
    end
    if unserved > 0 then
        f.want_t = (now > 0) and now or 1
        order_transports(view, memory, f, orders, unserved, wt, now)
    else
        f.want_t = 0
    end
end

-- the minute line (fy_minute)
local function minute(view, memory, f)
    if f.log_t > 0 and view.time >= f.log_t and view.time - f.log_t < 60000 then
        return
    end
    f.log_t = (view.time > 0) and view.time or 1
    local on = {0, 0, 0, 0}
    for _, t in ipairs(f.trips) do
        if t.phase > 0 then
            on[t.phase] = on[t.phase] + 1
        end
    end
    local sold, cits = {}, {}
    for _, cont in ipairs(f.order or {}) do
        local l = f.lands ~= nil and f.lands[cont] or nil
        if l ~= nil then
            sold[#sold + 1] = string.format("%d:%d/%d", cont, l.sold, l.want)
            cits[#cits + 1] = string.format("%d:%d/%d", cont, l.spare, l.free + l.wfree)
        end
    end
    print(string.format("ferry P%d at %.1f min: %d trips (%d soldiers, %d citizens put off; %d given up, %d transports sunk), %d transports trained, no free transport %d, none between the lands %d; on now: %d to the pick-up, %d boarding, %d crossing; %d sent home to a dock, %d citizens' trips too small, %d transports not ordered for want of a dock | soldiers stationed/share %s | citizens spare/places %s",
        view.player or 0, view.time / 60000.0, f.trips_n, f.soldiers, f.citizens, f.failed, f.sunk, f.trained,
        f.no_ship, f.no_shore, on[1], on[2], on[3], f.parked, f.small, f.no_dock,
        table.concat(sold, " "), table.concat(cits, " ")))
    if S.ferry_safety == true then
        local st = CX.stats(memory)
        print(string.format("ferry crossing: %d ways rated (%d off the line), trips held for the risk %d, for a load too small %d, important ones into danger %d; the worst of the last: %s",
            st.rated or 0, st.detours or 0, f.held, f.held_load, f.gone_high, CX.top()))
    end
    if S.sea_first == true and (f.way_t or 0) > 0 and view.time - f.way_t < FY_WAY_T then
        print(string.format("ferry sea_first: a way held for the risk at %d,%d to %d,%d (risk %d) - the squadrons are not ported: the trips wait for the risk to fall",
            f.way.a_x or 0, f.way.a_y or 0, f.way.b_x or 0, f.way.b_y or 0, f.way.risk or 0))
    end
    if (f.waves or 0) > 0 or (f.off or 0) > 0 then
        print(string.format("ferry across: %d push waves set out (%d transports), %d pushes called off at the beach",
            f.waves or 0, f.pushed or 0, f.off or 0))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The API

-- a scout asks the ferry for a crossing to (x, y) on another land (stations.c's scout list, offered to explore.lua)
function M.scout_ask(view, memory, id, x, y)
    if S.ferry_service ~= true or id == nil or x == nil or y == nil then
        return false
    end
    local f = state(memory)
    for k = 1, #f.asks do
        if f.asks[k].id == id then
            f.asks[k].x, f.asks[k].y, f.asks[k].t = x, y, view.time
            return true
        end
    end
    if #f.asks >= 4 then
        return false
    end
    f.asks[#f.asks + 1] = {id = id, x = x, y = y, t = view.time}
    return true
end

-- one step: the trips, every FY_LOOK the look, then the park (ferry_act)
function M.step(view, memory, orders)
    if S.ferry_service ~= true then
        return 0
    end
    orders = orders or {}
    local f = state(memory)
    memory.crossing = memory.crossing or {sunk = {}}
    local now = view.time
    for k = 1, FY_TRIPS do
        local t = f.trips[k]
        if t.phase ~= 0 then
            trip_act(view, memory, f, t, now)
        end
    end
    -- ACROSS: the soldiers of each push with a trip (walking to its shore, aboard) - the stations count them as still to
    -- cross, or a push whose every soldier was with the ferry was "nobody left"
    if S.push_across == true and S.expand_across == true then
        local counts = {}
        for k = 1, FY_TRIPS do
            local t = f.trips[k]
            if t.phase ~= 0 and t.kind == 3 and t.push >= 0 then
                counts[t.push] = (counts[t.push] or 0) + t.nsold
            end
        end
        SN.transit(memory, counts)
    end
    if f.look_t > 0 and now >= f.look_t and now - f.look_t < FY_LOOK then
        return f.n
    end
    f.look_t = (now > 0) and now or 1
    look(view, memory, f, orders, now)
    if S.transport_park == true then
        park(view, memory, f, now)
    end
    minute(view, memory, f)
    return f.n
end

return M

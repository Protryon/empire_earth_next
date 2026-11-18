-- New (AI_RESEARCH 8.96): classic_lua's war on water. require("classic_lua.sea"); main.lua steps it after the army;
-- army.lua's AssignSeaTask asks fishing_place (below) first.
--
-- THE SEA RAID. On an island map the land raid (army.lua raid_step, AI_RESEARCH 8.93) finds no enemy town on its land
-- and never goes: the pool's soldiers stayed home all game. The sea raid carries pool soldiers over on the transports
-- (lib.navy's landing: board, sail, unload) to the enemy's island, raids its gatherers there as the land raid does (a
-- lib.groups raid group, its home the beach, its targets on that land only) and brings the survivors home again (a
-- second lib.navy landing, to the own town's land), where the pool takes them in.
--   idle   -> from the Stone Age and RAID.start, every RAID.check_ms, on island and planets maps: no land raid out, no
--             enemy town known on the town's land (that is the land raid's), an enemy town known across the water
--             (else its building remembered last on land the town cannot walk to), a finished dock, RAID.min soldiers
--             in the pool, and an aim: the enemy's gather spot last seen on its land, no known tower within 9 tiles,
--             whose weakest beach (lib.navy N.beach) has no known armed enemy within 12. The free transports first:
--             while they hold fewer than the raid, one of the epoch's sea transports (ferry.lua's class) is ordered at
--             a time, RAID.max_ships at most; the raid goes with what they hold (RAID.min - RAID.keep at least): all
--             but RAID.keep of the pool, RAID.max at most
--   ships  -> the transports planned (lib.navy N.plan); none in RAID.ships_ms: given up
--   out    -> the landing at the aim's beach; aborted: the soldiers go back to the pool
--   raid   -> a lib.groups raid at the enemy, the aim first (g.same_land: its gather spots on the land the raid
--             stands on only; with none left, its buildings remembered there, not towers, RAID.reaims times); over when
--             it fled to the beach, has nowhere left to go, is down to a third or has been out RAID.out_ms
--   back   -> the soldiers hold until free transports carry them home; none in RAID.back_ms: they stay ashore (the
--             engine's: they fight what comes)
-- The next raid RAID.gap_ms after one ends. The transports a raid holds are left alone by the ferry and the army's
-- crossings (memory.cross_ships). Trace: `sea.raid ...` (`wait why=` once a minute while one is due and cannot go).
-- THE FLEET and THE SEA TASK: below. Unit-level commands only (ai.load, ai.unload, ai.move, ai.attack, through lib.navy
-- and lib.groups for the raid).
-- Memory: memory.sea = {raid = {...} or nil, next_raid = ms, fleet = {...}}.
local N = require("lib.navy")
local G = require("lib.groups")

local M = {}

M.RAID = {start = 20 * 60000, min = 10, max = 30, keep = 2, check_ms = 5000, gap_ms = 60000, out_ms = 300000,
          ships_ms = 240000, back_ms = 240000, max_ships = 3, reaims = 3}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- a tile's land and water (ai.tile: continent, sea ids; 0 none)
local function on_land(t)
    return t ~= nil and (t.continent or 0) ~= 0
end

local function on_water(t)
    return t ~= nil and (t.sea or 0) ~= 0 and (t.continent or 0) == 0
end

-- a known enemy tower (an armed building in the memory) within r of a tile (lib.groups' tower_near)
local function tower_near(x, y, r)
    for _, m in ipairs(ai.memory{buildings = true, near = {x, y, r}, limit = 8}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            return true
        end
    end
    return false
end

local function state(memory)
    local s = memory.sea
    if s == nil then
        s = {next_raid = 0}
        memory.sea = s
    end
    return s
end

local function army(memory)
    return memory.army
end

local function FERRY()
    return require("classic_lua.ferry")
end

local function U()
    return require("classic_lua.units")
end

-- the transports the ferry or an army crossing holds (memory.cross_ships: army.lua's crossings, refreshed each army
-- step; the ferry's trip)
local function ships_used(memory)
    local used = {}
    for _, id in ipairs(ai.keys(memory.cross_ships or {})) do
        used[id] = true
    end
    local f = memory.ferry
    if f ~= nil and f.trip ~= nil and f.trip.L ~= nil then
        for _, id in ipairs(f.trip.L.ships) do
            used[id] = true
        end
    end
    return used
end

-- the nearest enemy whose known town lies across the water (none on the own land: the land raid's case); an enemy
-- whose town is not seen yet: its building remembered last (ai.memory) on land the town cannot walk to. {player,
-- town_x, town_y}
local function target_enemy(view)
    local best, bd
    for _, e in ipairs(view.enemies or {}) do
        if e.town_x ~= nil and e.town_x >= 0 then
            if ai.reach(view.town_x, view.town_y, e.town_x, e.town_y) then
                return nil   -- (a land raid goes there)
            end
            local d = d2(e.town_x, e.town_y, view.town_x, view.town_y)
            if bd == nil or d < bd or (d == bd and e.player < best.player) then
                best, bd = {player = e.player, town_x = e.town_x, town_y = e.town_y}, d
            end
        end
    end
    if best ~= nil then
        return best
    end
    local bt
    for _, m in ipairs(ai.memory{buildings = true}) do
        local t = ai.tile(m.x, m.y)
        if on_land(t) and not ai.reach(view.town_x, view.town_y, m.x, m.y)
                and (bt == nil or m.seen > bt or (m.seen == bt and m.id < best.id)) then
            best, bt = {player = m.player, town_x = m.x, town_y = m.y, id = m.id}, m.seen
        end
    end
    return best
end

-- the landing's aim: the enemy's gather spots last seen on its land (the land of the town or building known; newest
-- first), away from its known towers, whose weakest beach on the dock's sea (lib.navy N.beach) has no known armed
-- enemy within 12 tiles. -> spot x, y, beach x, y; nil: none (a raid that lands by the enemy's town or towers only
-- turns back, and the transports sent for it were sunk)
local function aim(memory, e)
    local spots = ((army(memory) or {}).gspots or {})[e.player]
    if spots == nil then
        return nil
    end
    local sea = N.dock_sea()
    if sea == nil then
        return nil
    end
    local list = {}
    for _, k in ipairs(ai.keys(spots)) do
        local s = spots[k]
        if on_land(ai.tile(s.x, s.y)) and ai.reach(s.x, s.y, e.town_x, e.town_y) and not tower_near(s.x, s.y, 9) then
            list[#list + 1] = s
        end
    end
    table.sort(list, function(p, q)
        if p.t ~= q.t then
            return p.t > q.t
        end
        return p.x * 10000 + p.y < q.x * 10000 + q.y
    end)
    for _, s in ipairs(list) do
        local bx, by, str = N.beach({x = s.x, y = s.y}, sea)
        if bx ~= nil and str == 0 and not tower_near(bx, by, 9) then
            return s.x, s.y, bx, by
        end
    end
    return nil
end

-- the soldiers back to the pool (the engine; army.lua's pool_step takes in those on the town's land)
local function release(memory, r, why)
    local a = army(memory)
    local g = a ~= nil and G.get(a, r.group) or nil
    local n = 0
    if g ~= nil then
        n = #g.units
        G.disband(a, g)
    end
    print(string.format("classic_lua sea.raid end why=%s sent=%d back=%d", why, r.n or 0, n))
end

local function free_ships(memory)
    local used = ships_used(memory)
    local out = {}
    for _, t in ipairs(N.transports()) do
        if not used[t.id] and t.cargo == 0 then
            out[#out + 1] = t
        end
    end
    return out
end

local function start(view, memory, s)
    local o = M.RAID
    local a = army(memory)
    if a == nil or view.epoch < 1 or view.time < o.start or view.time < (s.next_raid or 0) or view.town_x < 0 then
        return
    end
    if not FERRY().water_map(memory) then
        return
    end
    -- (why it waits, once a minute)
    local function wait(why)
        if view.time - (s.wait_t or 0) >= 60000 then
            s.wait_t = view.time
            print(string.format("classic_lua sea.raid wait why=%s", why))
        end
    end
    if #ai.units{type = "b_dock", built = true} == 0 then
        return wait("no_dock")
    end
    if G.role(a, "raid") ~= nil then
        return
    end
    local pool = G.role(a, "pool")
    if pool == nil or #pool.units < o.min then
        return wait("pool_" .. (pool ~= nil and #pool.units or 0))
    end
    local e = target_enemy(view)
    if e == nil then
        return wait("no_town")
    end
    local ax, ay, bx, by = aim(memory, e)
    if ax == nil then
        return wait("no_spot")
    end
    local n = math.min(o.max, #pool.units - o.keep)
    -- the transports first: the raid leaves the pool only when the free ones hold it (one more ordered at a time,
    -- RAID.max_ships at most, while they do not)
    local cap, ships = 0, 0
    for _, t in ipairs(free_ships(memory)) do
        cap, ships = cap + t.cap, ships + 1
    end
    if cap < n then
        local class = FERRY().transport_class(view)
        if class ~= nil and #ai.units{type = class} < o.max_ships and not U().is_requested(memory, class)
                and not U().in_production(view, class) then
            U().request(view, memory, class, 1)
        end
        if cap < o.min - o.keep then
            return wait("ships_" .. ships)
        end
        n = cap
    end
    local ids = {}
    for k = 1, #pool.units do
        if #ids >= n then
            break
        end
        local u = ai.unit(pool.units[k])
        if u ~= nil and u.inside == nil then
            ids[#ids + 1] = u.id
        end
    end
    if #ids < o.min - o.keep then
        return
    end
    local g = G.create(a, {task = "defend", role = "searaid", target = e.player, x = view.town_x, y = view.town_y,
                           home_x = view.town_x, home_y = view.town_y, manual = true})
    G.add(a, g, ids)
    s.raid = {phase = "ships", t = view.time, group = g.id, enemy = e.player, n = #ids, ax = ax, ay = ay, bx = bx,
              by = by}
    print(string.format("classic_lua sea.raid start n=%d enemy=%d town=%d,%d pool=%d", #ids, e.player, e.town_x,
        e.town_y, #pool.units))
end

-- ships: enough free transports for the raid, else one more ordered at a time
local function ships_step(view, memory, s, r, g)
    local o = M.RAID
    if view.time - r.t >= o.ships_ms then
        s.raid = nil
        s.next_raid = view.time + o.gap_ms * 5
        release(memory, r, "no_ships")
        return
    end
    local free = free_ships(memory)
    local ships, cap = {}, 0
    for _, t in ipairs(free) do
        if cap < #g.units then
            ships[#ships + 1] = t
            cap = cap + t.cap
        end
    end
    if #ships > 0 and cap >= #g.units then
        local ax, ay = r.ax, r.ay
        local L, why = N.plan({x = ax, y = ay, player = r.enemy}, g.units, ships, {site_x = r.bx, site_y = r.by})
        if L == nil then
            s.raid = nil
            s.next_raid = view.time + o.gap_ms * 5
            release(memory, r, "plan_" .. tostring(why))
            return
        end
        r.L, r.phase, r.t = L, "out", view.time
        r.ships = #L.ships
        print(string.format("classic_lua sea.raid board ships=%d n=%d aim=%d,%d beach=%d,%d", #L.ships, #g.units, ax,
            ay, L.site_x, L.site_y))
        return
    end
    local class = FERRY().transport_class(view)
    if class == nil then
        return
    end
    local have = #ai.units{type = class}
    if have < o.max_ships and not U().is_requested(memory, class) and not U().in_production(view, class) then
        U().request(view, memory, class, 1)
    end
end

local function out_step(view, memory, s, r, g)
    local res, out = N.landing_step(r.L)
    if res == "landed" then
        g.manual = false
        g.task, g.role = "raid", "searaid"
        g.x, g.y = r.ax or -1, r.ay or -1   -- (the aim first: the gather spot or the building it was planned for)
        g.home_x, g.home_y = r.L.site_x, r.L.site_y
        g.same_land = true
        g.route, g.route_avoid = true, 8
        g.state, g.state_t, g.way_t = "moving", view.time, 0
        r.phase, r.t, r.landed = "raid", view.time, #out
        print(string.format("classic_lua sea.raid landed n=%d at=%d,%d", #out, r.L.site_x, r.L.site_y))
    elseif res == "aborted" then
        s.raid = nil
        s.next_raid = view.time + M.RAID.gap_ms
        release(memory, r, "aborted_" .. tostring(out))
    end
end

local function raid_step(view, memory, s, r, g)
    local o = M.RAID
    local alive = #g.units
    local why = nil
    if g.task ~= "raid" then
        why = "fled"
    elseif g.state == "arrived" then
        -- (no gather spot left on this land: the enemy's buildings remembered here, not towers, nearest first; at
        -- most RAID.reaims times)
        local cx, cy = g.cx or r.L.site_x, g.cy or r.L.site_y
        local best, bd
        if (r.reaims or 0) < o.reaims then
            for _, m in ipairs(ai.memory{buildings = true, player = r.enemy, near = {cx, cy, 40}}) do
                local c = ai.class(m.type)
                if c ~= nil and (c.attack or 0) == 0 and not (r.been or {})[m.id] and ai.reach(cx, cy, m.x, m.y) then
                    local d = d2(m.x, m.y, cx, cy)
                    if bd == nil or d < bd then
                        best, bd = m, d
                    end
                end
            end
        end
        if best ~= nil then
            r.reaims = (r.reaims or 0) + 1
            r.been = r.been or {}
            r.been[best.id] = true
            g.x, g.y = best.x, best.y
            g.state, g.state_t, g.way_t = "moving", view.time, 0
            return
        end
        why = "nowhere"
    elseif alive * 3 < (r.landed or r.n) then
        why = "spent"
    elseif view.time - r.t >= o.out_ms then
        why = "time"
    end
    if why == nil then
        return
    end
    r.why = why
    print(string.format("classic_lua sea.raid over why=%s ms=%d landed=%d alive=%d", why, view.time - r.t,
        r.landed or 0, alive))
    if alive == 0 then
        s.raid = nil
        s.next_raid = view.time + o.gap_ms
        release(memory, r, why)
        return
    end
    -- home: the soldiers hold where they are until the transports come (lib.groups' manual: the module orders them)
    g.manual = true
    ai.stop(g.units)
    r.phase, r.t, r.L = "back", view.time, nil
end

local function back_step(view, memory, s, r, g)
    local o = M.RAID
    if r.L == nil then
        if view.time - r.t >= o.back_ms then
            -- (no transport came: the soldiers stay ashore and fight what they meet, the engine's)
            s.raid = nil
            s.next_raid = view.time + o.gap_ms
            release(memory, r, "stranded")
            return
        end
        local free = free_ships(memory)
        local ships, cap = {}, 0
        for _, t in ipairs(free) do
            if cap < #g.units then
                ships[#ships + 1] = t
                cap = cap + t.cap
            end
        end
        if #ships > 0 then
            local L, why = N.plan({x = view.town_x, y = view.town_y, player = ai.me()}, g.units, ships)
            if L ~= nil then
                r.L = L
                print(string.format("classic_lua sea.raid pickup ships=%d n=%d", #L.ships, #g.units))
            elseif (r.noplan or 0) == 0 then
                r.noplan = 1
                print(string.format("classic_lua sea.raid pickup_noplan why=%s", tostring(why)))
            end
        end
        return
    end
    local res, out = N.landing_step(r.L)
    if res == "landed" then
        s.raid = nil
        s.next_raid = view.time + o.gap_ms
        release(memory, r, "home_" .. (r.why or "?"))
    elseif res == "aborted" then
        r.L = nil
        print(string.format("classic_lua sea.raid pickup_aborted why=%s", tostring(out)))
    end
end

-- the transports the sea raid holds (for the ferry and the army's crossings)
local function hold_ships(memory, r)
    if r ~= nil and r.L ~= nil and r.phase ~= "raid" then
        memory.cross_ships = memory.cross_ships or {}
        for _, id in ipairs(r.L.ships) do
            memory.cross_ships[id] = true
        end
    end
end

local fleet_step   -- (below: the fleet)

function M.step(view, memory)
    local s = state(memory)
    local a = army(memory)
    if a == nil then
        return
    end
    fleet_step(view, memory, s)
    local r = s.raid
    if r == nil then
        if view.time - (s.check_t or 0) >= M.RAID.check_ms then
            s.check_t = view.time
            start(view, memory, s)
        end
        return
    end
    local g = G.get(a, r.group)
    if g == nil or #g.units == 0 then
        if r.L ~= nil and r.phase ~= "raid" then
            -- (the transports go home)
            local ships = {}
            for _, id in ipairs(r.L.ships) do
                if ai.unit(id) ~= nil then
                    ships[#ships + 1] = id
                end
            end
            if #ships > 0 and r.L.board_wx ~= nil and r.phase == "out" then
                ai.move(ships, r.L.board_wx, r.L.board_wy, {forced = true})
            end
        end
        print(string.format("classic_lua sea.raid end why=lost phase=%s sent=%d back=0", r.phase, r.n or 0))
        s.raid = nil
        s.next_raid = view.time + M.RAID.gap_ms
        return
    end
    if r.phase == "ships" then
        ships_step(view, memory, s, r, g)
    elseif r.phase == "out" then
        out_step(view, memory, s, r, g)
    elseif r.phase == "raid" then
        raid_step(view, memory, s, r, g)
    elseif r.phase == "back" then
        back_step(view, memory, s, r, g)
    end
    hold_ships(memory, s.raid)
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE FLEET (the fishing war). On an island map the fishing boats are most of the food, both sides'. From the Stone
-- Age with a finished dock, the script keeps a fleet of its own beside army.lua's naval maintenance groups (a manual
-- lib.groups group, role "fleet"): FLEET.size warships, FLEET.per_epoch more an epoch after the Stone Age, FLEET.max at
-- most, of the best class the dock trains now (attack x hit points over the cost), the warships the engine holds
-- taken in and one more ordered at a time while it is short. Every FLEET.step_ms:
--   home   -> the seen enemy warships and armed buildings within 10 tiles outweigh it by 1.5: back to the water by the
--             own fishing boats (else the dock), the place avoided FLEET.avoid_ms
--   guard  -> enemy warships, transports, boats or citizens seen within FLEET.guard_r of the own fishing boats: the
--             fleet attacks them (warships first) unless their warships outweigh it by 1.5
--   fight  -> hunting or exploring, what it meets within FLEET.reach (warships, transports, boats and shore citizens,
--             docks), none within FLEET.tower_r of a known tower
--   hunt   -> at FLEET.hunt_k of its number, hit points half or more: the enemy's gather spot last seen on water or
--             on the shore of the fleet's sea (looked for every 10 s), else its dock remembered
--   scout  -> nothing known to hunt, FLEET.scout_min ships or more: a random point of its water (a new one on arrival
--             or after 90 s), which shows the enemy's coast to the raids and the hunt
-- Trace: `sea.fleet ...` every minute (`sunk` and `docks`: its targets gone from a visible tile, an estimate) and
-- `sea.kill`.
M.FLEET = {size = 6, per_epoch = 2, max = 12, hunt_k = 0.5, scout_min = 2, step_ms = 2000, guard_r = 14, reach = 10,
           tower_r = 8, start = 0, request_ms = 20000, avoid_ms = 180000}

local function warship_class(view)
    local best, bs
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building then
            local k = ai.class(c.type)
            if k ~= nil and k.domain == "sea" and k.category == "ship" and not k.is_transport and (k.attack or 0) > 0
                    and (k.speed or 0) > 0 then
                local cost = 0
                for _, v in pairs(k.cost or {}) do
                    cost = cost + v
                end
                local score = (k.attack or 0) * (k.hp or 1) * 1000 // math.max(1, cost)
                if bs == nil or score > bs or (score == bs and c.type < best) then
                    best, bs = c.type, score
                end
            end
        end
    end
    return best
end

-- the water by the own fishing boats (their centre), else by the dock
local function home_water(view)
    local boats = ai.units{category = "boat", limit = 16}
    if #boats > 0 then
        local cols = ai.get(boats, {"x", "y"})
        local sx, sy, n = 0, 0, 0
        for k = 1, #boats do
            if cols.x[k] ~= nil then
                sx, sy, n = sx + cols.x[k], sy + cols.y[k], n + 1
            end
        end
        if n > 0 then
            return sx // n, sy // n
        end
    end
    local _, dock = N.dock_sea()
    if dock ~= nil then
        return dock.x, dock.y
    end
    return nil
end

local function armed_strength(ids)
    local war = {}
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and (u.attack or 0) > 0 and (u.category == "ship" or u.category == "building") then
            war[#war + 1] = id
        end
    end
    return #war > 0 and ai.strength(war) or 0
end

-- a place the fleet turned back from in the last FLEET.avoid_ms (within 12 tiles)
local function avoided(f, x, y, now)
    for _, a in ipairs(f.avoid or {}) do
        if now - a.t < M.FLEET.avoid_ms and d2(a.x, a.y, x, y) <= 144 then
            return true
        end
    end
    return false
end

-- the enemy's fishing grounds and shore: its gather spots last seen on water (boats), or on land within 4 tiles of
-- the fleet's sea (the water there), newest first, nearest the fleet among the newest; else its docks remembered;
-- none by a known tower or where the fleet turned back lately
local function hunt_target(memory, f, cx, cy, now)
    local best, bt = nil, nil
    local here = ai.tile(cx, cy)
    local sea = here ~= nil and here.sea or nil
    for _, p in ipairs(ai.keys((army(memory) or {}).gspots or {})) do
        local spots = memory.army.gspots[p]
        for _, k in ipairs(ai.keys(spots)) do
            local s = spots[k]
            local t = ai.tile(s.x, s.y)
            local x, y = nil, nil
            if on_water(t) then
                x, y = s.x, s.y
            elseif on_land(t) and sea ~= nil and sea ~= 0 then
                local sx, sy = ai.shore(s.x, s.y, {sea = sea, max = 4})
                if sx ~= nil then
                    x, y = sx, sy
                end
            end
            if x ~= nil and not tower_near(s.x, s.y, M.FLEET.tower_r) and not avoided(f, x, y, now) then
                local score = s.t - d2(x, y, cx, cy) * 20
                if bt == nil or score > bt then
                    best, bt = {x = x, y = y, why = "spot"}, score
                end
            end
        end
    end
    if best ~= nil then
        return best
    end
    local bd
    for _, m in ipairs(ai.memory{buildings = true, type = "b_dock"}) do
        local d = d2(m.x, m.y, cx, cy)
        if (bd == nil or d < bd) and not avoided(f, m.x, m.y, now) then
            best, bd = {x = m.x, y = m.y, why = "dock", id = m.id}, d
        end
    end
    return best
end

-- the fleet (nil: no warship yet); the warships the engine holds join it up to its size. (lib.groups drops an empty
-- group: the fleet is made with its first ship.) army.lua's naval maintenance groups (CheckNaval's kind 5) keep their
-- ships: taken into the fleet, the enemy boats sunk on Small Islands fell from 220 to 84 in 32 games (AI_RESEARCH
-- 8.96; those groups roam the sea and fight what they meet)
local function claim(memory, size)
    local a = army(memory)
    local g = G.role(a, "fleet")
    local have = g ~= nil and #g.units or 0
    if have >= size then
        return g
    end
    local take = {}
    local cands = ai.units{category = "ship", held = "engine"}
    for _, id in ipairs(cands) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and (u.attack or 0) > 0 and have + #take < size then
            local k = ai.class(u.type)
            if k ~= nil and not k.is_transport then
                take[#take + 1] = id
            end
        end
    end
    if #take > 0 then
        if g == nil then
            g = G.create(a, {task = "defend", role = "fleet", manual = true})
        end
        G.add(a, g, take)
    end
    return g
end

local function fleet_targets(cx, cy, r, tower_r)
    local out = {}
    for _, id in ipairs(ai.seen{enemy = true, near = {cx, cy, r}}) do
        local u = ai.unit(id)
        if u ~= nil then
            local p = nil
            if u.category == "ship" and (u.attack or 0) > 0 then
                p = 0
            elseif u.category == "transport" then
                p = 1
            elseif u.category == "boat" or u.category == "citizen" then
                p = 2   -- (the citizens: gatherers on the shore within the warships' reach)
            elseif u.category == "building" and string.find(u.type or "", "dock", 1, true) then
                p = 3
            end
            if p ~= nil and p > 0 then
                if tower_near(u.x, u.y, tower_r) then
                    p = nil
                end
            end
            if p ~= nil then
                out[#out + 1] = {u = u, p = p, d = d2(u.x, u.y, cx, cy)}
            end
        end
    end
    table.sort(out, function(a, b)
        if a.p ~= b.p then
            return a.p < b.p
        end
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.u.id < b.u.id
    end)
    return out
end

fleet_step = function(view, memory, s)
    local o = M.FLEET
    if o.size <= 0 or not FERRY().water_map(memory) or view.epoch < 1 or view.time < o.start then
        return
    end
    if #ai.units{type = "b_dock", built = true} == 0 then
        return
    end
    local f = s.fleet
    if f == nil then
        f = {mode = "guard", t = 0, aims = {}, kills = 0, docks = 0}
        s.fleet = f
    end
    if view.time - f.t < o.step_ms then
        return
    end
    f.t = view.time
    -- (FLEET.size from the Stone Age, FLEET.per_epoch more an epoch after it, FLEET.max at most)
    local size = math.min(o.max, o.size + o.per_epoch * (view.epoch - 1))
    local g = claim(memory, size)
    -- the fleet kept: one more warship ordered at a time while short
    if (g == nil and 0 or #g.units) < size and view.time - (f.req_t or 0) >= o.request_ms then
        local class = warship_class(view)
        if class ~= nil and not U().is_requested(memory, class) and not U().in_production(view, class) then
            f.req_t = view.time
            U().request(view, memory, class, 1)
        end
    end
    -- the kills: the boats and docks it aimed at, gone from a visible tile
    for _, id in ipairs(ai.keys(f.aims)) do
        local a = f.aims[id]
        if ai.unit(id) == nil then
            local t = ai.tile(a.x, a.y)
            if t ~= nil and t.visible then
                if a.dock then
                    f.docks = f.docks + 1
                else
                    f.kills = f.kills + 1
                end
                print(string.format("classic_lua sea.kill %s at=%d,%d", a.dock and "dock" or a.kind, a.x, a.y))
            end
            f.aims[id] = nil
        elseif view.time - a.t > 60000 then
            f.aims[id] = nil
        end
    end
    if g == nil then
        return
    end
    local ships = {}
    local hp, hpmax = 0, 0
    for _, id in ipairs(g.units) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil then
            ships[#ships + 1] = u
            hp, hpmax = hp + (u.hp or 0), hpmax + (u.hp_max or u.hp or 1)
        end
    end
    if view.time % 60000 < o.step_ms then
        print(string.format("classic_lua sea.fleet n=%d mode=%s sunk=%d docks=%d", #ships, f.mode, f.kills, f.docks))
    end
    if #ships == 0 then
        return
    end
    local ids = {}
    local cx, cy = 0, 0
    for _, u in ipairs(ships) do
        ids[#ids + 1] = u.id
        cx, cy = cx + u.x, cy + u.y
    end
    cx, cy = cx // #ships, cy // #ships
    local own = ai.strength(ids)
    local hx, hy = home_water(view)
    -- danger here: the seen enemy warships and armed buildings within 10 tiles
    local near = ai.seen{enemy = true, armed = true, near = {cx, cy, 10}}
    local foe = armed_strength(near)
    local function go(x, y, mode)
        if f.mode ~= mode or f.gx ~= x or f.gy ~= y or view.time - (f.move_t or 0) >= 15000 then
            f.mode, f.gx, f.gy, f.move_t = mode, x, y, view.time
            ai.move(ids, x, y, {forced = true, formation = true})
        end
    end
    if foe * 2 > own * 3 then
        -- (the place is avoided a while; at most 8 places kept)
        if f.mode ~= "home" then
            f.avoid = f.avoid or {}
            table.insert(f.avoid, 1, {x = cx, y = cy, t = view.time})
            f.avoid[9] = nil
        end
        if hx ~= nil then
            go(hx, hy, "home")
        end
        return
    end
    -- guard: enemy naval units near the own fishing boats or docks
    if hx ~= nil then
        local threats = fleet_targets(hx, hy, o.guard_r, o.tower_r)
        if #threats > 0 then
            local foes = {}
            for _, c in ipairs(threats) do
                if c.p == 0 then
                    foes[#foes + 1] = c.u.id
                end
            end
            if ai.strength(foes) * 2 <= own * 3 then
                local t = threats[1].u
                if f.aim ~= t.id or view.time - (f.aim_t or 0) >= 8000 then
                    f.aim, f.aim_t, f.mode = t.id, view.time, "guard"
                    ai.attack(ids, t.id)
                    f.aims[t.id] = {x = t.x, y = t.y, t = view.time, kind = t.category,
                                    dock = t.category == "building"}
                end
                return
            end
        end
    end
    -- the targets within reach of the fleet itself
    local here = fleet_targets(cx, cy, o.reach, o.tower_r)
    if #here > 0 and (f.mode == "hunt" or f.mode == "scout" or here[1].p == 0) then
        local t = here[1].u
        if f.aim ~= t.id or view.time - (f.aim_t or 0) >= 8000 then
            f.aim, f.aim_t = t.id, view.time
            ai.attack(ids, t.id)
            f.aims[t.id] = {x = t.x, y = t.y, t = view.time, kind = t.category, dock = t.category == "building"}
        end
        return
    end
    -- hunt: the fleet full and not hurt
    local fit = hp * 2 >= hpmax
    -- (the target looked for again every 10 s: ai.shore per spot)
    if fit and (f.tg_t == nil or view.time - f.tg_t >= 10000) then
        f.tg, f.tg_t = hunt_target(memory, f, cx, cy, view.time), view.time
    end
    local tg = fit and f.tg or nil
    if fit and #ships >= math.ceil(size * o.hunt_k) then
        if tg ~= nil then
            if d2(cx, cy, tg.x, tg.y) <= 16 then
                -- (arrived: nothing here any more; the spot is forgotten when seen empty, lib.groups note_spots)
                if f.mode == "hunt" and view.time - (f.move_t or 0) >= 20000 and tg.id ~= nil then
                    ai.attack(ids, tg.id)
                    f.aims[tg.id] = {x = tg.x, y = tg.y, t = view.time, kind = "dock", dock = true}
                    f.move_t = view.time
                end
                return
            end
            go(tg.x, tg.y, "hunt")
            return
        end
    end
    -- nothing known to hunt: from FLEET.scout_min ships the fleet explores its water (a random point of it, a new one
    -- on arrival or after 90 s; ai.area_point with the script's draw)
    if fit and tg == nil and #ships >= o.scout_min then
        local ar = ai.area(cx, cy)
        if ar ~= nil and ar.category == 2 then
            if f.scout_x == nil or view.time - (f.scout_t or 0) >= 90000 or d2(cx, cy, f.scout_x, f.scout_y) <= 25 then
                f.scout_x, f.scout_y = ai.area_point(ar.id, ai.random(1, 0x7fffffff))
                f.scout_t = view.time
            end
            if f.scout_x ~= nil then
                go(f.scout_x, f.scout_y, "scout")
                return
            end
        end
    end
    if hx ~= nil and d2(cx, cy, hx, hy) > 36 then
        go(hx, hy, "guard")
    else
        f.mode = "guard"
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE SEA TASK (army.lua AssignSeaTask, M.SEA_TASK; island and planets maps): a naval sub-group's next place is the
-- enemy's fishing grounds on its water when known: the enemy's (the group's target player, any enemy for 0) gather
-- spot last seen on water of that body (a boat; lib.groups note_spots), newest first, none within FLEET.tower_r of a
-- known tower. The sub-group attacks there (lib.groups: the ships, then what it meets). nil: none known (the classic's
-- own ladder)
M.SEA_TASK = true

function M.fishing_place(memory, player, body)
    if not M.SEA_TASK or not FERRY().water_map(memory) then
        return nil
    end
    local gspots = (army(memory) or {}).gspots or {}
    local best = nil
    for _, p in ipairs(ai.keys(gspots)) do
        if player == 0 or p == player then
            for _, k in ipairs(ai.keys(gspots[p])) do
                local s = gspots[p][k]
                if (best == nil or s.t > best.t) and on_water(ai.tile(s.x, s.y)) then
                    local ar = ai.area(s.x, s.y)
                    if ar ~= nil and ar.id == body and not tower_near(s.x, s.y, M.FLEET.tower_r) then
                        best = s
                    end
                end
            end
        end
    end
    if best == nil then
        return nil
    end
    return best.x, best.y
end

return M

-- Age of AI port, the walls (upstream dll/ai/defence.c: WALLS, ENCLOSE, DOCK POCKETS; the fronts' wall fields of
-- def_refresh, def_scan's walls, gate_fits, encl_plan, encl_walls, encl_stone / encl_owed, defence_tick's wall
-- part). GPL-3.0, Grille and the Age of AI contributors; see UPSTREAM and LICENSE in this folder.
--
-- WALLS ([ai_base] defence_walls, default on): walls belong in front of towers. Every 20 s per AI: once 2 towers
-- stand within 7 tiles of a front's point (defence_distance tiles from the base towards the nearest enemy building
-- known, the two nearest enemies, else the map's centre - econ.lua's towers stand there), an 11-tile wall goes up 3
-- tiles in front of them, across the front at the nearest of the 8 directions (along the tiles, or exactly diagonal
-- with 8 steps), 3 tries 3 minutes apart, 11 x the wall's cost in stock, the newest wall it can build, not where
-- known enemy defences cover or an enemy army stands, on land and on the map at both ends and the middle, not when
-- 8 pieces already stand on the arc; the ends stay open (own units walk round, attackers on foot break through or
-- walk its length under tower fire). A gate in its middle once the 5 pieces there stand finished, 20 looks.
-- ENCLOSE ([ai_base] enclose, 0 upstream "until measured"): a plan every minute. The home continent flood-filled from
-- the base: at most ENCL_ISLAND tiles and no enemy building known on it - an island, its ring the coast (its land
-- tiles with water beside them); else an octagon round the town (its sides along the tiles and exactly diagonal) 3
-- tiles beyond its farthest building within 30, at least defence_distance + 2, 32 at most, never shrinking, its
-- water tiles left out. Within 2 tiles of an own dock the ring stays open. The ring nearest the own buildings first.
-- Walls (every 20 s): the ring only once enclose_citizens citizens (the economy first), enclose_runs straight runs a
-- tick (8 directions, at most 12 pieces, what the stock above 100 stone pays), a gate in the middle of each of the
-- octagon's four sides once its pieces stand. Towers: the layout's slots become spots along the ring (on the coast
-- 1 tile inland and 5 apart, in the octagon 2 inside and 6 apart; econ.lua reads them), the front walls are not
-- built. Stone: the walls first (the extra towers wait for the open ring's stone, up to 400; the economy puts
-- workers on stone for the open ring + 200: econ.lua's wants). SAFE SPACE (enclose_safe_tiles): an island with
-- that many land tiles beyond ENCL_SAFE of the sea is not walled. DOCK POCKETS (enclose_dock_gates): the coast
-- within 4 tiles of a dock stays open and the pocket gets a square wall of its own ENCL_POCKET tiles round the dock
-- with a gate in the middle of its side towards the town.
-- Engine: ai.wall(citizens, class, x1, y1, x2, y2) is the human's drag (the pieces are foundations the citizens
-- build; the order takes them, they are released here when they stop); the builders are the port's pick (upstream's
-- wall manager chose them): the 3 (a ring run: 2) free citizens nearest the wall. ai.gate(class, x, y) turns the 5
-- finished pieces into a gate as the human's gate button does. The land and the water are ai.tile (fog-honest: an
-- unexplored tile is neither, so a continent is an island only once all its land is explored; upstream read the
-- map's areas). The wall and gate types are the classic wall manager's pick by the epoch (the engine's "wall" /
-- "gate" aliases), upstream's line (Copper onward), a piece 10 stone.
local S = require("ageofai.settings")
local V = require("lib.view")

local M = {}

-- the wall and gate lines (upstream's UT_WALL_LINE / UT_GATE_LINE, the newest the player can build): the classic
-- wall manager's pick by the epoch (EECPCivilizationManager GetWallClass / GetGateClass, the engine's "wall" / "gate"
-- aliases; the explicit name here because ai.can_place takes no alias), the same line - nothing before the Copper
-- Age, as upstream. A wall is never "available" the production way (view.costs has none), and ai.class carries no
-- cost: a piece costs WALL_STONE (dbtechtree: every wall 10 stone)
local WALL_STONE = 10
local WALL_EPOCH = 2   -- the Copper Age
local WALL_LINE = {[2] = "b_wall_copper", [3] = "b_wall_bronze", [4] = "b_wall_bronze", [5] = "b_wall_middle",
                   [6] = "b_wall_middle", [7] = "b_wall_imperial", [8] = "b_wall_imperial", [9] = "b_wall_ww1",
                   [10] = "b_wall_ww1", [11] = "b_wall_ww1", [12] = "b_wall_digital", [13] = "b_wall_digital",
                   [14] = "b_wall_space"}
local GATE_LINE = {[2] = "b_gate_copper", [3] = "b_gate_bronze", [4] = "b_gate_bronze", [5] = "b_gate_middle",
                   [6] = "b_gate_middle", [7] = "b_gate_imperial", [8] = "b_gate_imperial", [9] = "b_gate_ww1",
                   [10] = "b_gate_ww1", [11] = "b_gate_ww1", [12] = "b_gate_digital", [13] = "b_gate_digital",
                   [14] = "b_gate_space"}
local ENCL_RING, ENCL_ISLAND, ENCL_SAFE, ENCL_POCKET = 2048, 8000, 9, 5
local FILL_STEP = 600    -- DEVIATION (the step's budget): tiles of the flood fill looked at a step (a continent of
                         -- ENCL_ISLAND tiles takes 14 steps; upstream filled in one call)
local FREE_LOOKS = 80    -- DEVIATION (the step's budget): placement tests a tick for a run's first tile; a tile
                         -- refused stays off for 5 minutes (BAD_MS)
local BAD_MS = 300000
local KEY = 4096         -- a tile's key: y * KEY + x (the maps are smaller)
local step5 = {{0, 0}, {1, 0}, {0, 1}, {-1, 1}, {1, 1}}   -- gate_fits' five tiles along dir 1..4
local AXES = {{1, 0}, {0, 1}, {1, 1}, {1, -1}}

local function say(fmt, ...)
    print("walls " .. string.format(fmt, ...))
end

local function round(v)   -- lroundf: half away from zero
    if v >= 0 then
        return math.floor(v + 0.5)
    end
    return -math.floor(-v + 0.5)
end

local function tdiv(a, b)   -- C's division: towards zero
    local q = a // b
    if a % b ~= 0 and (a < 0) ~= (b < 0) then
        q = q + 1
    end
    return q
end

local function key_of(x, y)
    return y * KEY + x
end

local function newest(view, line)   -- the newest of the line the player can build now (nil before the Copper Age)
    if view.epoch < WALL_EPOCH then
        return nil
    end
    return line[math.min(view.epoch, 14)]
end


local function is_wall(t)
    return t:sub(1, 7) == "b_wall_"
end

local function is_gate(t)
    return t:sub(1, 7) == "b_gate_"
end

local function cost_of(view, type)   -- a wall piece's cost (a gate turns finished pieces: nothing)
    local out = {}
    for _, r in ipairs(V.resources) do
        out[r] = 0
    end
    if is_wall(type) then
        out.stone = WALL_STONE
    end
    return out
end

-- enemy soldiers it sees within r (raiders_near); with ships, the warships too
local function raiders_near(x, y, r, ships)
    local ids = ai.seen{enemy = true, armed = true, near = {x, y, r}}
    local cols = ai.get(ids, {"category"})
    for k = 1, #ids do
        local c = cols.category[k]
        if c == "soldier" or c == "hero" or (ships and c == "ship") then
            return true
        end
    end
    return false
end

-- route_covered: the tile, or the way from the base to it (every 6 tiles), under known enemy defences (the threat
-- layer stands in for upstream's tower_covers along the route)
local function route_covered(bx, by, x, y)
    if (ai.danger(x, y) or 0) > 0 then
        return true
    end
    local len = math.max(math.abs(x - bx), math.abs(y - by))
    local steps = math.max(len // 6, 1)
    for k = 1, steps - 1 do
        if (ai.danger(bx + (x - bx) * k // steps, by + (y - by) * k // steps) or 0) > 0 then
            return true
        end
    end
    return false
end

-- a tile's land and water (ai.tile, cached per plan: continent * 65536 + sea, -1 unexplored, nil off the map)
local function tile_code(cache, x, y)
    local k = key_of(x, y)
    local v = cache ~= nil and cache[k] or nil
    if v ~= nil then
        return v
    end
    local t = ai.tile(x, y)
    if t == nil then
        return nil
    end
    if t.continent == nil then
        v = -1
    else
        v = t.continent * 65536 + (t.sea or 0)
    end
    if cache ~= nil then
        cache[k] = v
    end
    return v
end

local function code_land(v)
    return v ~= nil and v > 0 and v // 65536 or 0
end

local function code_water(v)
    return v ~= nil and v > 0 and v % 65536 ~= 0
end

-- ---------------------------------------------------------------------------------------------------------------
-- def_scan: the own buildings (towers, wall and gate pieces, docks, the rest) once a tick

local function scan(view)
    local s = {towers = {}, walls = {}, bld = {}, docks = {}, walled = {}, piece = {}}
    local ids = ai.units{category = "building"}
    local cols = ai.get(ids, {"type", "x", "y", "built", "hp", "armed"})
    for k = 1, #ids do
        local t, x, y = cols.type[k], cols.x[k], cols.y[k]
        if t ~= nil and x ~= nil and x >= 0 and y >= 0 then
            if is_wall(t) or is_gate(t) then
                local key = key_of(x, y)
                local built = cols.built[k] == true and (cols.hp[k] or 0) > 0
                s.walls[#s.walls + 1] = {x = x, y = y}
                s.walled[key] = (s.walled[key] or 0) + 1
                s.piece[key] = {gate = is_gate(t), built = built}
            elseif (cols.hp[k] or 0) > 0 then
                s.bld[#s.bld + 1] = {x = x, y = y}
                if cols.armed[k] and t:find("tower", 1, true) ~= nil then
                    s.towers[#s.towers + 1] = {x = x, y = y}
                end
                if t == "b_dock" and #s.docks < 16 then
                    s.docks[#s.docks + 1] = {x = x, y = y}
                end
            end
        end
    end
    return s
end

-- gate_fits: the five tiles at (x, y) along dir each hold exactly one piece, a finished wall piece of its own (not
-- a gate); build_gate reads past its list on fewer
local function gate_fits(s, map, x, y, dir)
    if dir < 1 or dir > 4 then
        return false
    end
    local sx, sy = step5[dir + 1][1], step5[dir + 1][2]
    for i = -2, 2 do
        local tx, ty = x + sx * i, y + sy * i
        if tx < 0 or ty < 0 or tx >= map.width or ty >= map.height then
            return false
        end
        local key = key_of(tx, ty)
        local p = s.piece[key]
        if (s.walled[key] or 0) ~= 1 or p == nil or p.gate or not p.built then
            return false
        end
    end
    return true
end

-- on_segment: (x, y) within 1 tile of the segment a-b, sampled at 11 points
local function on_segment(x, y, ax, ay, bx, by)
    for j = 0, 10 do
        if math.abs(x - (ax + tdiv((bx - ax) * j, 10))) <= 1 and math.abs(y - (ay + tdiv((by - ay) * j, 10))) <= 1 then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- The builders: the free citizens nearest the wall (held by the order, released when they stop)

local function builders(x, y, n)
    local ids = ai.units{category = "citizen", held = "engine", near = {x, y, 80}, limit = 24}
    local cols = ai.get(ids, {"action", "inside", "x", "y"})
    local out = {}
    for k = 1, #ids do
        if #out >= n then
            break
        end
        if cols.x[k] ~= nil and cols.inside[k] == nil and cols.action[k] ~= "build" and ai.reach(x, y, cols.x[k], cols.y[k]) then
            out[#out + 1] = ids[k]
        end
    end
    return out
end

local function builders_note(w, ids, now)
    for _, id in ipairs(ids) do
        w.bld[#w.bld + 1] = {id = id, t = now}
    end
end

local function builders_release(w, now)
    local keep, free = {}, {}
    for _, b in ipairs(w.bld) do
        local u = ai.unit(b.id)
        if u == nil or u.action == "idle" or now - b.t >= 240000 then
            free[#free + 1] = b.id
        else
            keep[#keep + 1] = b
        end
    end
    if #free > 0 then
        ai.release(free)
    end
    w.bld = keep
end

-- ---------------------------------------------------------------------------------------------------------------
-- def_refresh (every 30 s): the two fronts - towards the nearest building known of the two nearest enemies, else
-- the map's centre; the wall state kept while the front stays within 2 tiles

local function refresh(view, w, now)
    if w.t ~= nil and now >= w.t and now - w.t < 30000 then
        return
    end
    w.t = now
    local old = w.f or {}
    w.f = {}
    w.nf = 0
    if view.town_x < 0 then
        return
    end
    local bx, by = view.town_x, view.town_y
    local near = {}   -- the nearest building known of each enemy
    for _, p in ipairs(ai.players()) do
        if p.enemy and p.alive then
            local bld = ai.memory{buildings = true, player = p.id, near = {bx, by, 400}, limit = 1}
            if #bld > 0 then
                local dx, dy = bld[1].x - bx, bld[1].y - by
                near[#near + 1] = {id = p.id, x = bld[1].x, y = bld[1].y, d2 = dx * dx + dy * dy}
            end
        end
    end
    local used = -1
    for k = 1, 2 do
        local best = nil
        for _, e in ipairs(near) do
            if e.id ~= used and (best == nil or e.d2 < best.d2) then
                best = e
            end
        end
        local dx, dy
        if best ~= nil then
            dx, dy = best.x - bx, best.y - by
        elseif k == 1 then
            local map = ai.map()
            dx, dy = map.width // 2 - bx, map.height // 2 - by
        else
            break
        end
        local len = math.sqrt(dx * dx + dy * dy)
        if len < 4.0 then
            break
        end
        local ux, uy = dx / len, dy / len
        local f = {enemy = best ~= nil and best.id or -1, dx = round(ux * 1000), dy = round(uy * 1000),
                   fx = bx + round(ux * S.defence_distance), fy = by + round(uy * S.defence_distance),
                   wall_tries = 0, wall_t = 0, gx = 0, gy = 0, gdir = 0, gtries = 0}
        for _, o in ipairs(old) do
            if math.abs(o.fx - f.fx) <= 2 and math.abs(o.fy - f.fy) <= 2 then
                f.wall_tries, f.wall_t = o.wall_tries, o.wall_t
                f.gx, f.gy, f.gdir, f.gtries = o.gx, o.gy, o.gdir, o.gtries
            end
        end
        used = f.enemy
        w.f[#w.f + 1] = f
        w.nf = #w.f
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- ENCLOSE: the plan (encl_plan, every minute)

local function encl_state(w)
    w.e = w.e or {t = 0, log_t = 0, island = false, r = 0, tiles = 0, walled = 0, rx = {}, ry = {}, sx = {}, sy = {},
                  gate = {}, pend = {}, runs = 0, pieces = 0, gates = 0, stone = 0, ready = false, safe = 0,
                  sheltered = false, bad = {}, skip_pend = 0, skip_fire = 0, skip_raid = 0, skip_stone = 0, skip_econ = 0}
    return w.e
end

-- the flood fill of the home continent, a chunk a step (FILL_STEP tiles): land of hc that is not water, or a tile
-- with an own wall piece; stops at ENCL_ISLAND (over) and notes an unexplored tile met (unknown)
local function fill_start(e, bx, by, hc, walled)
    e.fill = {qx = {bx}, qy = {by}, head = 1, mark = {[key_of(bx, by)] = true}, tl = {}, hc = hc, over = false,
              unknown = false, done = false, walled = {}}
    for key, n in pairs(walled) do   -- (read only: a set)
        if n > 0 then
            e.fill.walled[key] = true
        end
    end
end

local function fill_step(e, map)
    local f = e.fill
    if f == nil or f.done then
        return
    end
    local walled = f.walled
    local n = 0
    local nb4 = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
    while f.head <= #f.qx and not f.over and n < FILL_STEP do
        local x, y = f.qx[f.head], f.qy[f.head]
        f.head = f.head + 1
        n = n + 1
        for _, d in ipairs(nb4) do
            local qx, qy = x + d[1], y + d[2]
            local key = key_of(qx, qy)
            if qx >= 0 and qy >= 0 and qx < map.width and qy < map.height and not f.mark[key] then
                f.mark[key] = true
                local v = tile_code(f.tl, qx, qy)
                if v == -1 then
                    f.unknown = true
                end
                local land = code_land(v) == f.hc and not code_water(v)
                if land or walled[key] then
                    if #f.qx >= ENCL_ISLAND then
                        f.over = true
                        break
                    end
                    f.qx[#f.qx + 1] = qx
                    f.qy[#f.qy + 1] = qy
                end
            end
        end
    end
    if f.head > #f.qx or f.over then
        f.done = true
    end
end

local function encl_land(cache, hc, map, walled, x, y)   -- land of the continent, off the map's edge; a walled tile kept
    if x < 3 or y < 3 or x >= map.width - 3 or y >= map.height - 3 then
        return false
    end
    local v = tile_code(cache, x, y)
    return (code_land(v) == hc and not code_water(v)) or (walled[key_of(x, y)] or 0) > 0
end

local function encl_coast(cache, x, y)   -- water in the 8 tiles round it
    for dx = -1, 1 do
        for dy = -1, 1 do
            if (dx ~= 0 or dy ~= 0) and code_water(tile_code(cache, x + dx, y + dy)) then
                return true
            end
        end
    end
    return false
end

-- the enemy buildings known on the home continent (encl_scan's enemy)
local function enemy_on(view, hc)
    local bld = ai.memory{buildings = true, enemy = true, near = {view.town_x, view.town_y, 400}, limit = 64}
    for _, b in ipairs(bld) do
        local t = ai.tile(b.x, b.y)
        if t ~= nil and t.continent == hc then
            return true
        end
    end
    return false
end

local function encl_plan(view, w, s, now)
    local e = encl_state(w)
    local map = ai.map()
    if view.town_x < 0 then
        return
    end
    local bx, by = view.town_x, view.town_y
    local hc = (ai.tile(bx, by) or {}).continent or 0
    -- phase 1: a new plan every minute: an enemy on the continent -> no fill (the octagon), else the fill starts
    if hc ~= 0 and (e.t == 0 or (now >= e.t and now - e.t >= 60000)) and e.fill == nil then
        e.t = now
        e.enemy = enemy_on(view, hc)
        if e.enemy then
            e.fill = {done = true, over = true, unknown = false, qx = {}, qy = {}, tl = {}, hc = hc, head = 1, mark = {}, walled = {}}
        else
            fill_start(e, bx, by, hc, s.walled)
            fill_step(e, map)
        end
    end
    local f = e.fill
    if f == nil or not f.done then
        return
    end
    e.fill = nil
    if f.hc ~= hc then
        return
    end
    local cache = f.tl
    local k = {}
    e.tiles = #f.qx
    -- the island: filled whole, under ENCL_ISLAND tiles, no enemy building on it; the fill met no unexplored tile
    -- (fog-honest: the land beyond may run on)
    e.island = not e.enemy and not f.over and not f.unknown
    -- SAFE SPACE: the land beyond ENCL_SAFE tiles of the sea, by a walk inward from the coast (only asked for with
    -- enclose_safe_tiles; the mines' count is upstream's log only)
    e.safe = 0
    if e.island and S.enclose_safe_tiles > 0 then
        local dist, wx, wy = {}, {}, {}
        for h = 1, #f.qx do
            dist[key_of(f.qx[h], f.qy[h])] = 255
        end
        for h = 1, #f.qx do
            if encl_coast(cache, f.qx[h], f.qy[h]) then
                dist[key_of(f.qx[h], f.qy[h])] = 1
                wx[#wx + 1], wy[#wy + 1] = f.qx[h], f.qy[h]
            end
        end
        local h = 1
        while h <= #wx and #wx < ENCL_ISLAND do
            local d0 = dist[key_of(wx[h], wy[h])]
            for dx = -1, 1 do
                for dy = -1, 1 do
                    local key = key_of(wx[h] + dx, wy[h] + dy)
                    if dist[key] == 255 then
                        dist[key] = math.min(d0 + 1, 254)
                        wx[#wx + 1], wy[#wy + 1] = wx[h] + dx, wy[h] + dy
                    end
                end
            end
            h = h + 1
        end
        for q = 1, #f.qx do
            if dist[key_of(f.qx[q], f.qy[q])] > ENCL_SAFE then
                e.safe = e.safe + 1
            end
        end
    end
    e.sheltered = e.island and S.enclose_safe_tiles > 0 and e.safe >= S.enclose_safe_tiles
    if e.sheltered then
        e.island = false
    end
    local gates = {}
    if e.island then   -- the coast
        for h = 1, #f.qx do
            local x, y = f.qx[h], f.qy[h]
            if #k < ENCL_RING and encl_land(cache, hc, map, s.walled, x, y) and encl_coast(cache, x, y) then
                k[#k + 1] = {x = x, y = y}
            end
        end
        if S.enclose_dock_gates then   -- the dock pockets' walls and gates
            local seen = {}
            for _, q in ipairs(k) do
                seen[key_of(q.x, q.y)] = true
            end
            for d = 1, math.min(#s.docks, 4) do
                local o = s.docks[d]
                for dx = -ENCL_POCKET, ENCL_POCKET do
                    for dy = -ENCL_POCKET, ENCL_POCKET do
                        if (math.abs(dx) == ENCL_POCKET or math.abs(dy) == ENCL_POCKET) and #k < ENCL_RING then
                            local qx, qy = o.x + dx, o.y + dy
                            if not seen[key_of(qx, qy)] and encl_land(cache, hc, map, s.walled, qx, qy) then
                                seen[key_of(qx, qy)] = true
                                k[#k + 1] = {x = qx, y = qy}
                            end
                        end
                    end
                end
                local gx, gy = bx - o.x, by - o.y   -- the side towards the town
                local g
                if math.abs(gx) >= math.abs(gy) then
                    g = {x = o.x + (gx > 0 and ENCL_POCKET or -ENCL_POCKET), y = o.y, dir = 2, tries = 0}
                else
                    g = {x = o.x, y = o.y + (gy > 0 and ENCL_POCKET or -ENCL_POCKET), dir = 1, tries = 0}
                end
                if encl_land(cache, hc, map, s.walled, g.x, g.y) then
                    gates[#gates + 1] = g
                end
            end
        end
    end
    if not e.island and not e.sheltered then   -- the octagon round the town
        local reach = 0
        for _, b in ipairs(s.bld) do
            local dx, dy = math.abs(b.x - bx), math.abs(b.y - by)
            local d = math.max(dx, dy)
            local dd = math.ceil((dx + dy) / 1.414)
            if dd > d then
                d = dd
            end
            if d <= 30 and d > reach then
                reach = d
            end
        end
        local r = reach + 3
        if r < S.defence_distance + 2 then
            r = S.defence_distance + 2
        end
        if r > 32 then
            r = 32
        end
        if r < e.r then
            r = e.r
        end
        e.r = r
        local sd = round(r * 1.414)
        local function inside(dx, dy)
            return math.abs(dx) <= r and math.abs(dy) <= r and math.abs(dx) + math.abs(dy) <= sd
        end
        for dx = -r, r do
            for dy = -r, r do
                if #k < ENCL_RING and inside(dx, dy)
                        and not (inside(dx + 1, dy) and inside(dx - 1, dy) and inside(dx, dy + 1) and inside(dx, dy - 1)) then
                    if encl_land(cache, hc, map, s.walled, bx + dx, by + dy) then
                        k[#k + 1] = {x = bx + dx, y = by + dy}
                    end
                end
            end
        end
        for _, side in ipairs({{1, 0, 2}, {-1, 0, 2}, {0, 1, 1}, {0, -1, 1}}) do
            local qx, qy = bx + side[1] * r, by + side[2] * r
            if encl_land(cache, hc, map, s.walled, qx, qy) then
                gates[#gates + 1] = {x = qx, y = qy, dir = side[3], tries = 0}
            end
        end
    end
    -- the gates' tries kept while a gate stays
    for _, g in ipairs(gates) do
        for _, o in ipairs(e.gate) do
            if o.x == g.x and o.y == g.y then
                g.tries = o.tries
            end
        end
    end
    e.gate = gates
    -- open at the docks; the rest by distance to the nearest own building
    local wide = (e.island and S.enclose_dock_gates) and ENCL_POCKET - 1 or 2
    local m = {}
    for _, q in ipairs(k) do
        local open = false
        for _, d in ipairs(s.docks) do
            if math.abs(d.x - q.x) <= wide and math.abs(d.y - q.y) <= wide then
                open = true
                break
            end
        end
        if not open then
            local best = 0x7FFFFFFF
            for _, b in ipairs(s.bld) do
                local dx, dy = b.x - q.x, b.y - q.y
                local d2 = dx * dx + dy * dy
                if d2 < best then
                    best = d2
                end
            end
            m[#m + 1] = {x = q.x, y = q.y, key = best}
        end
    end
    table.sort(m, function(a, b)
        if a.key ~= b.key then
            return a.key < b.key
        elseif a.x ~= b.x then
            return a.x < b.x
        end
        return a.y < b.y
    end)
    local ringset = {}
    for _, q in ipairs(m) do
        ringset[key_of(q.x, q.y)] = true
    end
    local walled, gap, inn = 0, e.island and 5 or 6, e.island and 1 or 2
    e.rx, e.ry, e.sx, e.sy = {}, {}, {}, {}
    for i, q in ipairs(m) do
        e.rx[i], e.ry[i] = q.x, q.y
        if (s.walled[key_of(q.x, q.y)] or 0) > 0 then
            walled = walled + 1
        end
        if #e.sx < 96 then
            -- a tower spot: inward towards the base point (on the coast 1 or 2 tiles, the first off the ring)
            local sx, sy, ok = 0, 0, false
            for d = inn, inn + 1 do
                if not ok then
                    sx = q.x + d * (bx > q.x and 1 or bx < q.x and -1 or 0)
                    sy = q.y + d * (by > q.y and 1 or by < q.y and -1 or 0)
                    ok = encl_land(cache, hc, map, s.walled, sx, sy) and (s.walled[key_of(sx, sy)] or 0) == 0
                         and not ringset[key_of(sx, sy)]
                end
            end
            for j = 1, #e.sx do
                if not ok then
                    break
                end
                ok = math.abs(e.sx[j] - sx) >= gap or math.abs(e.sy[j] - sy) >= gap
            end
            if ok then
                e.sx[#e.sx + 1], e.sy[#e.sy + 1] = sx, sy
            end
        end
    end
    e.walled = walled
    local wt = newest(view, WALL_LINE)
    e.stone = wt ~= nil and (#m - walled) * cost_of(view, wt).stone or 0
    if e.log_t == 0 or now - e.log_t >= 300000 then
        e.log_t = now
        say("enclose %s continent=%d ring=%d walled=%d spots=%d gates=%d docks=%d runs=%d pieces=%d gates_built=%d skipped=%d/%d/%d/%d/%d safe=%d stone=%d",
            e.sheltered and "sheltered" or e.island and "island" or "town-ring", e.tiles, #m, walled, #e.sx, #e.gate, #s.docks,
            e.runs, e.pieces, e.gates, e.skip_pend, e.skip_fire, e.skip_raid, e.skip_stone, e.skip_econ, e.safe, e.stone)
        e.skip_pend, e.skip_fire, e.skip_raid, e.skip_stone, e.skip_econ = 0, 0, 0, 0, 0
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- ENCLOSE: the walls and gates (encl_walls, every 20 s)

local function run_tiles(p)   -- the tiles of an ordered run
    local dx, dy = p.bx - p.ax, p.by - p.ay
    local len = math.max(math.abs(dx), math.abs(dy))
    local sx, sy = dx > 0 and 1 or dx < 0 and -1 or 0, dy > 0 and 1 or dy < 0 and -1 or 0
    return len, sx, sy
end

local function encl_pending(e, x, y, now)   -- on a run ordered in the last 4 minutes
    for _, p in ipairs(e.pend) do
        if now - p.t < 240000 then
            local len, sx, sy = run_tiles(p)
            for j = 0, len do
                if p.ax + sx * j == x and p.ay + sy * j == y then
                    return true
                end
            end
        end
    end
    return false
end

local function encl_in_flight(e, s, now)   -- the runs ordered in the last 4 minutes with a piece not standing finished
    local n = 0
    for _, p in ipairs(e.pend) do
        if now - p.t < 240000 then
            local len, sx, sy = run_tiles(p)
            for j = 0, len do
                local q = s.piece[key_of(p.ax + sx * j, p.ay + sy * j)]
                if q == nil or not q.built then
                    n = n + 1
                    break
                end
            end
        end
    end
    return n
end

local function encl_walls(view, w, s, now)
    local e = encl_state(w)
    local map = ai.map()
    encl_plan(view, w, s, now)
    if #e.rx == 0 then
        e.ready = false
        return
    end
    e.ready = view.citizens >= S.enclose_citizens
    if not e.ready then
        e.skip_econ = e.skip_econ + 1
        return
    end
    for _, g in ipairs(e.gate) do   -- a gate, once the side's middle stands finished: the newest it can build
        if g.tries < 20 and gate_fits(s, map, g.x, g.y, g.dir) then
            local gtype = newest(view, GATE_LINE)
            g.tries = g.tries + 1
            if gtype ~= nil then
                local code, reason = ai.gate(gtype, g.x, g.y)
                g.tries = 20
                e.gates = e.gates + 1
                say("enclose gate type=%s at=%d,%d result=%s reason=%s", gtype, g.x, g.y, tostring(code), tostring(reason))
                return
            end
        end
    end
    local wtype = newest(view, WALL_LINE)
    if wtype == nil then
        return
    end
    local wc = cost_of(view, wtype)
    if S.enclose_in_flight > 0 and encl_in_flight(e, s, now) >= S.enclose_in_flight then
        e.skip_pend = e.skip_pend + 1
        return
    end
    local stock = {}
    for _, r in ipairs(V.resources) do
        stock[r] = view.stock[r]
    end
    local ringset = {}
    for i = 1, #e.rx do
        ringset[key_of(e.rx[i], e.ry[i])] = true
    end
    local looks = 0
    local function free(x, y)
        local key = key_of(x, y)
        if not ringset[key] or (s.walled[key] or 0) > 0 or encl_pending(e, x, y, now) then
            return false
        end
        if e.bad[key] ~= nil and now - e.bad[key] < BAD_MS then
            return false
        end
        looks = looks + 1
        if ai.can_place(wtype, x, y) then
            return true
        end
        e.bad[key] = now
        return false
    end
    for _ = 1, S.enclose_runs do
        local most = 12   -- the pieces the stock pays for
        for _, r in ipairs(V.resources) do
            local c, have = wc[r], stock[r] - (r == "stone" and 100 or 0)
            if c > 0 and tdiv(have, c) < most then
                most = tdiv(have, c)
            end
        end
        if most < 1 then
            e.skip_stone = e.skip_stone + 1
            return
        end
        local ax, ay, bx, by = -1, -1, -1, -1
        for i = 1, #e.rx do
            if ax >= 0 or looks >= FREE_LOOKS then
                break
            end
            local x, y = e.rx[i], e.ry[i]
            if not free(x, y) then
                if (s.walled[key_of(x, y)] or 0) == 0 and encl_pending(e, x, y, now) then
                    e.skip_pend = e.skip_pend + 1
                end
            elseif route_covered(view.town_x, view.town_y, x, y) then
                e.skip_fire = e.skip_fire + 1   -- the builders would walk into enemy fire
            elseif raiders_near(x, y, 10, not (S.enclose_under_ships and e.island)) then
                e.skip_raid = e.skip_raid + 1   -- or an enemy army, ships off the coast
            else
                local best = 0
                for _, a in ipairs(AXES) do
                    local f, bk, dx, dy = 0, 0, a[1], a[2]
                    while f + 1 < most and free(x + dx * (f + 1), y + dy * (f + 1)) do
                        f = f + 1
                    end
                    while f + bk + 1 < most and free(x - dx * (bk + 1), y - dy * (bk + 1)) do
                        bk = bk + 1
                    end
                    if f + bk + 1 > best then
                        best = f + bk + 1
                        ax, ay, bx, by = x - dx * bk, y - dy * bk, x + dx * f, y + dy * f
                    end
                end
            end
        end
        if ax < 0 then
            return
        end
        local len = math.max(math.abs(bx - ax), math.abs(by - ay))
        local ids = builders((ax + bx) // 2, (ay + by) // 2, 2)
        if #ids == 0 then
            return
        end
        local code, reason = ai.wall(ids, wtype, ax, ay, bx, by)
        if code ~= "ok" then
            say("enclose wall refused type=%s from=%d,%d to=%d,%d reason=%s", wtype, ax, ay, bx, by, tostring(reason))
            return
        end
        builders_note(w, ids, now)
        local o, oldest = 0, nil
        if #e.pend < 24 then
            o = #e.pend + 1
        else
            for i, p in ipairs(e.pend) do
                if oldest == nil or p.t < oldest then
                    o, oldest = i, p.t
                end
            end
        end
        e.pend[o] = {ax = ax, ay = ay, bx = bx, by = by, t = now}
        e.runs = e.runs + 1
        e.pieces = e.pieces + len + 1
        for _, r in ipairs(V.resources) do
            stock[r] = stock[r] - (len + 1) * wc[r]
        end
        say("enclose wall type=%s from=%d,%d to=%d,%d pieces=%d builders=%d ring=%d walled=%d", wtype, ax, ay, bx, by, len + 1,
            #ids, #e.rx, e.walled)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- defence_tick's walls (every 20 s): the gate in a front's wall, then the front's wall

local function front_walls(view, w, s, now)
    local map = ai.map()
    for k = 1, w.nf do   -- the gate, once the wall's middle stands finished: the newest it can build
        local f = w.f[k]
        if f.gdir ~= 0 and f.gtries < 20 and gate_fits(s, map, f.gx, f.gy, f.gdir) then
            local gtype = newest(view, GATE_LINE)
            f.gtries = f.gtries + 1
            if gtype ~= nil then
                local code, reason = ai.gate(gtype, f.gx, f.gy)
                f.gdir = 0
                w.gates = (w.gates or 0) + 1
                say("gate type=%s at=%d,%d front=%d,%d result=%s reason=%s", gtype, f.gx, f.gy, f.fx, f.fy, tostring(code), tostring(reason))
                return   -- one order per tick
            end
        end
    end
    for k = 1, w.nf do
        local f = w.f[k]
        if f.wall_tries < 3 and (f.wall_t == 0 or now - f.wall_t >= 180000) then
            local ntow = 0
            for _, t in ipairs(s.towers) do
                if math.abs(t.x - f.fx) <= 7 and math.abs(t.y - f.fy) <= 7 then
                    ntow = ntow + 1
                end
            end
            local wtype = ntow >= 2 and newest(view, WALL_LINE) or nil
            local ok = wtype ~= nil
            if ok then
                local wc = cost_of(view, wtype)
                for _, r in ipairs(V.resources) do
                    if view.stock[r] < 11 * wc[r] then
                        ok = false
                    end
                end
            end
            if ok then
                local ux, uy = f.dx / 1000, f.dy / 1000
                local cx, cy = f.fx + round(ux * 3), f.fy + round(uy * 3)
                local px, py = -uy, ux   -- across the front, snapped to 8 directions
                local apx, apy = math.abs(px), math.abs(py)
                local sx = apx > 2.414 * apy and (px > 0 and 1 or -1) or (apy > 2.414 * apx and 0 or (px > 0 and 1 or -1))
                local sy = apy > 2.414 * apx and (py > 0 and 1 or -1) or (apx > 2.414 * apy and 0 or (py > 0 and 1 or -1))
                local h = (sx ~= 0 and sy ~= 0) and 4 or 5   -- 11 tiles along, 8 diagonal steps (11.3)
                local ax, ay, bx, by = cx + sx * h, cy + sy * h, cx - sx * h, cy - sy * h
                local tx, ty = view.town_x, view.town_y
                if route_covered(tx, ty, ax, ay) or route_covered(tx, ty, bx, by) or route_covered(tx, ty, cx, cy) then
                    ok = false   -- the builders would walk into enemy fire
                elseif raiders_near(ax, ay, 10) or raiders_near(bx, by, 10) or raiders_near(cx, cy, 10) then
                    ok = false   -- or into an enemy army standing there
                else
                    -- the arc on the map and on land, ends and middle (an arc into the water crashed the game's wall
                    -- placement upstream)
                    local land = true
                    for _, q in ipairs({{ax, ay}, {bx, by}, {cx, cy}}) do
                        if land then
                            local v = (q[1] >= 3 and q[2] >= 3 and q[1] < map.width - 3 and q[2] < map.height - 3)
                                      and tile_code(nil, q[1], q[2]) or nil
                            land = code_land(v) ~= 0 and not code_water(v)
                        end
                    end
                    if not land then
                        f.wall_tries = f.wall_tries + 1
                        f.wall_t = now
                        ok = false
                    else
                        local have = 0   -- wall and gate pieces standing on the arc (then a new order only repairs)
                        for _, p in ipairs(s.walls) do
                            if on_segment(p.x, p.y, ax, ay, bx, by) then
                                have = have + 1
                            end
                        end
                        if have >= 8 then
                            ok = false
                        else
                            local ids = builders(cx, cy, 3)
                            if #ids > 0 then
                                local code, reason = ai.wall(ids, wtype, ax, ay, bx, by)
                                f.wall_tries = f.wall_tries + 1
                                f.wall_t = now
                                if code == "ok" then
                                    builders_note(w, ids, now)
                                    f.gx, f.gy = cx, cy
                                    f.gdir = sy == 0 and 1 or sx == 0 and 2 or sx == sy and 4 or 3
                                    f.gtries = 0
                                    w.built = (w.built or 0) + 1
                                end
                                say("wall type=%s from=%d,%d to=%d,%d towers=%d front=%d,%d builders=%d result=%s reason=%s",
                                    wtype, ax, ay, bx, by, ntow, f.fx, f.fy, #ids, tostring(code), tostring(reason))
                                return   -- one order per tick
                            end
                        end
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step: the fill goes on every step; the rest every 20 s (defence_tick). eco gets the stone the ring owes
-- (econ.c encl_stone: the open walls + 200 for repairs once the ring is on; encl_owed: the extra towers wait for
-- the open walls' stone) and the ring's tower spots (def_slot with enclose ready)

function M.step(view, memory)
    if not S.defence_layout or (not S.defence_walls and not S.enclose) then
        return
    end
    memory.walls = memory.walls or {t = nil, nf = 0, f = {}, tick_t = 0, bld = {}, built = 0, gates = 0}
    local w = memory.walls
    local now = view.time
    if S.enclose and w.e ~= nil and w.e.fill ~= nil and not w.e.fill.done then
        fill_step(w.e, ai.map())
    end
    if w.tick_t ~= 0 and now >= w.tick_t and now - w.tick_t < 20000 then
        return
    end
    w.tick_t = now
    builders_release(w, now)
    refresh(view, w, now)
    if view.town_x < 0 then
        return
    end
    local s = scan(view)
    local eco = memory.eco or {}
    eco.wall_stone, eco.wall_owed, eco.ring_spots = 0, 0, nil
    if S.enclose then
        encl_walls(view, w, s, now)
        local e = w.e
        eco.wall_owed = e.stone
        if #e.rx > 0 and e.ready then
            eco.wall_stone = e.stone + 200
            if #e.sx > 0 then
                eco.ring_spots = {}
                for i = 1, #e.sx do
                    eco.ring_spots[i] = {e.sx[i], e.sy[i]}
                end
            end
            return   -- the ring instead of the fronts' walls
        end
    end
    if S.defence_walls then
        front_walls(view, w, s, now)
    end
end

return M

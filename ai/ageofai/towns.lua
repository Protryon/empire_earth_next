-- Age of AI port, the towns and the outposts (upstream dll/ai/towns.c the towns on the home continent, dll/ai/expand.c
-- the expansion to other islands; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI
-- contributors). require("ageofai.towns"). Hooked from main.lua after the economy's step.
--
-- towns.c (the [ai_base] towns bits, settings.lua): 1 RESOURCE TOWNS - every 30 s from the Copper Age with 20+
-- workers, no enemy soldiers in the base and its army at least the enemy's it knows, when the mines near its drop
-- sites are full (fewer than 4 free places) the richest explored mine group of the home continent that no other
-- player's drop site is within 18 tiles of, none of its own within 32, no enemy building it knows within 22 and no
-- enemy defence covers, 32 to 90 tiles from the base (nearer counting more): a Town Center (a Settlement when it
-- can't pay one; 2 SETTLEMENT: always a Settlement), then towers_per_town towers 5 tiles from it on the side of the
-- enemy it knows (else round it). towns_citizens citizens nearest the spot walk there and build (all on the drop site
-- until it stands, 3 on each tower); once the drop site stands they gather at its mines. A building destroyed is
-- laid again (3 times); the town lost (no building and no citizen of it left) - that spot again after 8 minutes,
-- three losses: never. 16 SPREAD: no waiting for the home mines, no count limit (15 workers a town, one going up
-- per 50 workers, only with a surplus), the map in cells of 8 tiles scored by its explored resources (a mine 3, a
-- bush 2, a tree 1, 12 trees a cell at most), the spot nearest to its towns within 45 tiles first, never more than
-- 20 tiles ahead of its front town; 2 towers and 1 per 4 mines, 5 houses, a wall 60+ tiles out. towns_care: the
-- spot and its way safe (no enemy soldiers known within 20, out of the enemy's reach and the lost fights, the way
-- from its nearest town clear of threats and 22 tiles of enemy buildings). 8 FORWARD BASE: where the army is (the
-- force's staging point with 8+ soldiers, else its biggest group of 8+ in an 8-tile cell 32+ tiles out, pulled back
-- to the first tile out of the enemy's reach), 32+ tiles from the base and every town, its army 0.8 x the enemy's,
-- a tower it can pay: towns_citizens + 2 citizens put up two towers towards the front, a Hospital, a production
-- building behind the towers, two towers on the flanks and an 11-tile wall 3 tiles in front of the two front
-- towers; then they go back. The reserve (towns_reserve): the drop site a town waits for (a Town Center for 90 s,
-- then a Settlement, 3 minutes at most), its next tower once the drop site is laid, a forward base's two towers and
-- Hospital while it saves.
--
-- expand.c (the expand_islands bits): 1 SETTLE - from the Copper Age and minute expand_minute, once its miners fill
-- half the home island's mine places (or from minute 15), with 25+ workers and its army not weaker than the enemy's
-- it knows: the island with the most mines it has explored (10 a mine, a quarter a tile of distance, 15 an enemy
-- building known there) that holds no ally's building (4 ALLY: those count as free), no enemy town or defence it
-- knows, at most 3 enemy buildings and no more enemy soldiers known within 20 tiles of its mines than its guard;
-- never one lost three times (8 minutes after a loss). A transport (its own, idle and empty, the last trip's first,
-- else trained), expand_citizens citizens nearest the home shore and expand_guard land soldiers board at the home
-- shore nearest the base and the island (the load once the transport is there and 3 in 4 are near, or after 2
-- minutes; sails when all are aboard, 7 in 10 after 90 s of loading, or with 3 citizens after 4 minutes; the sea
-- held - called off and unloaded at home) and unload at the island's shore nearest its mines. Ashore: a Settlement
-- by the mines (all on it until it stands), a tower between the mines and the sea, one by the Settlement (2 BARRACKS:
-- a barracks too; 128 FOLLOW: a ring of four towers, a guard of 4, only while it holds the sea and its army is
-- 1.5 x the enemy's); once the Settlement stands the citizens gather at the island's free mines, the guard stays by
-- it and attacks what it sees within 14 tiles; more citizens (expand_trips in all) every 2 minutes while the island
-- has free places, no enemy was seen there for a minute and the army is not outmatched. Lost (nothing of it there
-- 20 s): again after 8 minutes, 3 losses: never. 8 EARLY: from expand_minute without waiting for the home mines,
-- from 20 workers. 32 MORE: once an outpost holds it is left to its own gatherers and the next island is planned (at
-- most expand_max, the nearest to what it holds). 64 FORWARD: an outpost nearer the enemy's known buildings than
-- home gets a barracks and two more towers while the AI holds the sea.
--
-- Not ported (UPSTREAM says so): towns.c 4 FORWARD PRODUCTION, 32 SCOUT, 64 GROW, 128 FORT, 256 TAKE MINES and
-- 512 MINE RAIDS (armies.c's), the sites' memory and the crew; expand.c 16 EXTRA (the citizens' goal), 256 FORT,
-- 512 GUNS, the empty transport's scouting trip (phase 4: here an island's mines are known only once explored),
-- the stranded citizens' rescue and citizens_useful, the islands log.
local V = require("lib.view")
local N = require("lib.navy")
local S = require("ageofai.settings")
local M = {}

local FARM_EPOCH = 2
local TN_PROJ = 24
local TN_SPREAD_WORKERS = 15
local TN_SPREAD_BUILDING = 50
local TN_CELL = 8
local TN_CELL_WOOD = 12
local TN_CELL_MIN = 3
local TN_COMPACT = 45
local TN_FRONT_STEP = 20
local TN_FAR_WALL = 60
local TN_HOUSES = 5
local TN_CARE_TRIES = 24
local TN_CARE_SOLDIERS = 20
local TN_CARE_PAUSE = 300000
local TK_DROP, TK_TOWER, TK_PROD, TK_HOUSE = 1, 2, 3, 4
local K_NAME = {"drop site", "tower", "production building", "house"}
local EX_PROJ = 8
local XK_TOWER, XK_DROP, XK_BARRACKS = 1, 2, 3
local XK_NAME = {"tower", "drop site", "barracks"}
local DROP_TYPES = {b_town_center = true, b_capitol = true, b_settlement = true}
local TOWN_TYPES = {b_town_center = true, b_capitol = true}
local MINERALS = {"stone", "gold", "iron"}
-- the wall classes, oldest first (UT_WALL_LINE)
local WALLS = {"b_wall_palisades", "b_wall_copper", "b_wall_bronze", "b_wall_middle", "b_wall_imperial", "b_wall_ww",
               "b_wall_digital", "b_wall_information", "b_wall_space"}
-- the towers' angles round the drop site (towns.c: (0.6 + 0.6 x nth / 2) radians to either side, x 2.6 with no
-- enemy known): cos and sin in thousandths (the runtime has no math.sin)
local ROT = {{825, 565}, {362, 932}, {-227, 974}, {-737, 675}, {-990, 141}, {-897, -443}}
local ROT_WIDE = {{11, 1000}, {-1000, 22}, {-36, -999}, {999, -43}, {54, 999}, {-998, 64}}

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

local function dist(x1, y1, x2, y2)
    return isqrt(d2(x1, y1, x2, y2))
end

local function has(bits, b)
    return (bits or 0) & b ~= 0
end

local function round_div(v, d)
    if v >= 0 then
        return (v + d // 2) // d
    end
    return -((-v + d // 2) // d)
end

-- the land mass of a tile (a tile on none, a mine or a tree, takes a neighbour's); 0 unexplored or water
local function land_near(x, y)
    local t = ai.tile(x, y)
    if t ~= nil and (t.continent or 0) ~= 0 then
        return t.continent
    end
    for _, d in ipairs({{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}, {2, 0}, {0, 2}, {-2, 0}, {0, -2}}) do
        t = ai.tile(x + d[1], y + d[2])
        if t ~= nil and (t.continent or 0) ~= 0 then
            return t.continent
        end
    end
    return 0
end

-- the water tile beside a shore tile on the sea
local function water_by(x, y, sea)
    for _, d in ipairs({{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {2, 0}, {-2, 0}, {0, 2}, {0, -2}}) do
        local t = ai.tile(x + d[1], y + d[2])
        if t ~= nil and t.sea ~= nil and t.sea ~= 0 and (sea == nil or t.sea == sea) then
            return x + d[1], y + d[2]
        end
    end
    return nil
end

-- [priority.c] the memory of the step under way, for the list's refusals (prio_refuses: a town's building is a
-- building like any other; towns.c tn_run asks it before each project)
local prio_memory = nil
local function can_pay(view, type)
    local c = V.cost(view, type)
    if c == nil then
        return false
    end
    for _, r in ipairs(V.resources) do
        if view.stock[r] < c.cost[r] then
            return false
        end
    end
    if prio_memory ~= nil and require("ageofai.priority").refuses(view, prio_memory, type) ~= nil then
        return false
    end
    return true
end

local function tower_class(view)
    return require("ageofai.econ").tower_class(view)
end

local function wall_class(view)
    for k = #WALLS, 1, -1 do
        if V.cost(view, WALLS[k]) ~= nil then
            return WALLS[k]
        end
    end
    return nil
end

-- a type's cost per resource (0 when it cannot be made)
local function cost_of(view, type)
    local c = V.cost(view, type)
    local out = {}
    for _, r in ipairs(V.resources) do
        out[r] = c ~= nil and c.cost[r] or 0
    end
    return out
end

-- the enemy army it knows (the memory's armed units seen in the last 10 minutes) against its own land army, as hit
-- points x attack (ex_outmatched: upstream's strength is sqrt(hp x attack) a side; the port's measure throughout)
local function outmatched(view)
    local theirs = 0
    for _, m in ipairs(ai.memory{since = view.time - 600000}) do
        if not m.building then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                theirs = theirs + m.hp * c.attack
            end
        end
    end
    local mine = ai.strength(ai.units{category = "soldier"})
    return theirs > mine, mine, theirs
end

-- enemy soldiers known within r of a tile
local function raiders_known(x, y, r)
    local n = 0
    for _, m in ipairs(ai.memory{near = {x, y, r}}) do
        if not m.building then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                n = n + 1
            end
        end
    end
    return n
end

-- enemy soldiers seen now within r of a tile
local function raiders_near(x, y, r)
    local ids = ai.seen{enemy = true, armed = true, near = {x, y, r}}
    local cols = ai.get(ids, {"category"})
    local n = 0
    for k = 1, #ids do
        local c = cols.category[k]
        if c == "soldier" or c == "hero" then
            n = n + 1
        end
    end
    return n
end

-- in the enemy's reach it knows, or where it lost a fight (stage.c stage_danger)
local function stage_danger(x, y)
    return (ai.danger(x, y) or 0) > 0 or (ai.danger(x, y, "lost") or 0) > 0
end

local function say(fmt, ...)
    print("towns " .. string.format(fmt, ...))
end

-- ---------------------------------------------------------------------------------------------------------------
-- The world: the drop sites of the other players it knows, its own (and its towns going up), the enemy buildings it
-- knows (tn_world; fog-honest: the memory and the allies in sight, not the game's grid)

local function world(mem, view)
    local w = {drop = {}, own = {}, foe = {}}
    local ids = ai.units{category = "building"}
    local cols = ai.get(ids, {"type", "x", "y"})
    for k = 1, #ids do
        if DROP_TYPES[cols.type[k] or ""] and cols.x[k] ~= nil then
            w.own[#w.own + 1] = {cols.x[k], cols.y[k]}
        end
    end
    for _, t in ipairs(mem.towns) do
        if t.state ~= 0 then
            w.own[#w.own + 1] = {t.x, t.y}
        end
    end
    for _, m in ipairs(ai.memory{buildings = true}) do
        w.foe[#w.foe + 1] = {m.x, m.y}
        if DROP_TYPES[m.type] then
            w.drop[#w.drop + 1] = {m.x, m.y}
        end
    end
    local allies = ai.seen{enemy = false, category = "building"}
    cols = ai.get(allies, {"type", "x", "y"})
    for k = 1, #allies do
        if DROP_TYPES[cols.type[k] or ""] and cols.x[k] ~= nil then
            w.drop[#w.drop + 1] = {cols.x[k], cols.y[k]}
        end
    end
    return w
end

local function within(list, x, y, r)
    local rr = r * r
    for _, p in ipairs(list) do
        if d2(p[1], p[2], x, y) < rr then
            return true
        end
    end
    return false
end

-- the nearest of a list: its distance (nil for an empty list)
local function nearest(list, x, y)
    local bd, bp = nil, nil
    for _, p in ipairs(list) do
        local d = d2(p[1], p[2], x, y)
        if bd == nil or d < bd then
            bd, bp = d, p
        end
    end
    return bd, bp
end

local function bad_spot(mem, x, y, now, losses)
    for _, b in ipairs(mem.bad) do
        if d2(b.x, b.y, x, y) < 12 * 12 and (b.losses >= losses or now < b.again) then
            return true
        end
    end
    return false
end

-- the explored mines on the home continent (tn_site's list): id, x, y, room
local function home_mines(view, hc, r)
    local out = {}
    local bx, by = view.town_x, view.town_y
    for _, kind in ipairs(MINERALS) do
        local ids = ai.resources{kind = kind, near = {bx, by, r}}
        local cols = ai.get(ids, {"x", "y", "amount", "has_room"})
        for k = 1, #ids do
            if (cols.amount[k] or 0) > 0 and ai.reach(bx, by, cols.x[k], cols.y[k]) then
                out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], room = cols.has_room[k] and true or false}
            end
        end
    end
    return out
end

-- free places at the mines near its drop sites (tn_free_slots: the home continent's within 16 tiles of one of them;
-- here the view's sites, which lie within the claim range)
local function free_slots(view)
    local free = 0
    for _, s in ipairs(view.sites) do
        if s.units > 0 and s.resource ~= nil and s.resource ~= "food" and s.resource ~= "wood" and s.has_drop then
            free = free + math.max(s.capacity - s.workers, 0)
        end
    end
    return free
end

-- the gatherers a mine holds (the game's table 0x16): the view's mineral sites say it
local function mine_cap(view)
    for _, s in ipairs(view.sites) do
        if s.resource ~= nil and s.resource ~= "food" and s.resource ~= "wood" and s.capacity > 0 then
            return s.capacity
        end
    end
    return 8
end

-- ---------------------------------------------------------------------------------------------------------------
-- The site (tn_site): the home continent's best free mine group - its middle and its mines

local function tn_site(mem, view, hc, w, now)
    local mines = home_mines(view, hc, 100)
    local bx, by = view.town_x, view.town_y
    local best, bsc, bn = nil, 0, 0
    for _, m in ipairs(mines) do
        local db = dist(m.x, m.y, bx, by)
        local ok = db >= 32 and db <= 90
        ok = ok and not within(w.drop, m.x, m.y, 18) and not within(w.own, m.x, m.y, 32)
        ok = ok and not within(w.foe, m.x, m.y, 22)
        ok = ok and not bad_spot(mem, m.x, m.y, now, 3) and (ai.danger(m.x, m.y) or 0) == 0
        if ok then
            local n, sx, sy = 0, 0, 0
            for _, o in ipairs(mines) do
                if d2(m.x, m.y, o.x, o.y) <= 100 then
                    n, sx, sy = n + 1, sx + o.x, sy + o.y
                end
            end
            local sc = n * 3000 // (30 + db)
            if sc > bsc then
                best, bsc, bn = {x = sx // n, y = sy // n}, sc, n
            end
        end
    end
    return best, bn
end

-- towns_care part 1 (tn_safe): the spot and its way from its nearest town
local function tn_safe(view, w, px, py, mines, compact)
    local care = S.towns_care and 1 or 0
    if has(care, 2) and not compact and mines < 2 then
        return false
    end
    if not has(care, 1) then
        return true
    end
    if raiders_known(px, py, TN_CARE_SOLDIERS) > 0 or stage_danger(px, py) then
        return false
    end
    local fx, fy = view.town_x, view.town_y
    local bd = d2(px, py, fx, fy)
    for _, o in ipairs(w.own) do
        local d = d2(px, py, o[1], o[2])
        if d < bd then
            bd, fx, fy = d, o[1], o[2]
        end
    end
    -- the way: enemy soldiers it knows and their towers along it (the threat layer every 6 tiles)
    local len = dist(fx, fy, px, py)
    local steps = math.max(len // 6, 1)
    for k = 1, steps - 1 do
        local x, y = fx + (px - fx) * k // steps, fy + (py - fy) * k // steps
        if (ai.danger(x, y) or 0) > 0 then
            return false
        end
    end
    -- 22 tiles of the enemy buildings it knows: the way's nearest point to each
    local dx, dy = px - fx, py - fy
    local l2 = dx * dx + dy * dy
    for _, f in ipairs(w.foe) do
        local t = 0
        if l2 > 0 then
            t = ((f[1] - fx) * dx + (f[2] - fy) * dy) * 1000 // l2
            t = math.max(0, math.min(1000, t))
        end
        local ex, ey = fx + dx * t // 1000 - f[1], fy + dy * t // 1000 - f[2]
        if ex * ex + ey * ey < 22 * 22 then
            return false
        end
    end
    return true
end

-- SPREAD's cells (tn_site_spread): the explored resources of the home continent summed per 8-tile cell, cached for
-- two minutes (the trees are many). A cell: {ix, sc, n, sx, sy, mines}
local function spread_cells(mem, view, hc, now)
    if mem.cells ~= nil and now - (mem.cells_t or 0) < 120000 then
        return mem.cells, mem.cells_side
    end
    local map = ai.map()
    local side = math.min((map.width + TN_CELL - 1) // TN_CELL, 128)
    local cells = {}
    local function add(x, y, worth, wood, mine)
        local cx, cy = math.min(x // TN_CELL, side - 1), math.min(y // TN_CELL, side - 1)
        local ix = cy * side + cx
        local c = cells[ix]
        if c == nil then
            c = {ix = ix, sc = 0, n = 0, sx = 0, sy = 0, wood = 0, mines = 0, land = nil}
            cells[ix] = c
        end
        if wood > 0 and c.wood >= TN_CELL_WOOD then
            return
        end
        c.sc = c.sc + worth
        c.wood = c.wood + wood
        c.mines = c.mines + mine
        c.n = c.n + 1
        c.sx = c.sx + x
        c.sy = c.sy + y
    end
    local bx, by = view.town_x, view.town_y
    local far = map.width + map.height
    for _, kind in ipairs(MINERALS) do
        local ids = ai.resources{kind = kind, near = {bx, by, far}}
        local cols = ai.get(ids, {"x", "y", "amount"})
        for k = 1, #ids do
            if (cols.amount[k] or 0) > 0 and ai.reach(bx, by, cols.x[k], cols.y[k]) then
                add(cols.x[k], cols.y[k], 3, 0, 1)
            end
        end
    end
    do   -- berries (animals walk off, fish swim)
        local ids = ai.resources{kind = "food", near = {bx, by, far}}
        local cols = ai.get(ids, {"x", "y", "amount", "type"})
        for k = 1, #ids do
            if cols.type[k] == "res_forage_bush" and (cols.amount[k] or 0) > 0 then
                add(cols.x[k], cols.y[k], 2, 0, 0)
            end
        end
    end
    do   -- the trees nearest the base (a deviation: upstream reads every tree of the map; the charge bounds it here)
        local ids = ai.resources{kind = "wood", near = {bx, by, far}, limit = 1500}
        local cols = ai.get(ids, {"x", "y"})
        for k = 1, #ids do
            if cols.x[k] ~= nil then
                add(cols.x[k], cols.y[k], 1, 1, 0)
            end
        end
    end
    -- the cells on the home continent only (one look at each cell's middle; the bushes and trees were not checked)
    local list = {}
    for _, ix in ipairs(ai.keys(cells)) do
        local c = cells[ix]
        if c.mines > 0 or ai.reach(bx, by, c.sx // c.n, c.sy // c.n) then
            c.land = nil
            list[#list + 1] = c
        end
    end
    mem.cells, mem.cells_side, mem.cells_t = list, side, now
    return list, side
end

local function tn_site_spread(mem, view, hc, w, now)
    local list, side = spread_cells(mem, view, hc, now)
    local bx, by = view.town_x, view.town_y
    local byix = {}
    for _, c in ipairs(list) do
        byix[c.ix] = c
    end
    -- the front: its town nearest to an enemy building it knows
    local front = -1
    if #w.foe > 0 then
        local pts = {{bx, by}}
        for _, o in ipairs(w.own) do
            pts[#pts + 1] = o
        end
        for _, q in ipairs(pts) do
            local d = nearest(w.foe, q[1], q[2])
            d = isqrt(d)
            if front < 0 or d < front then
                front = d
            end
        end
    end
    local cand = {}
    for _, c in ipairs(list) do
        local sc, mines = 0, 0
        local gx, gy = c.ix % side, c.ix // side
        for dy = -1, 1 do
            for dx = -1, 1 do
                if gx + dx >= 0 and gx + dx < side and gy + dy >= 0 and gy + dy < side then
                    local e = byix[(gy + dy) * side + gx + dx]
                    if e ~= nil then
                        sc, mines = sc + e.sc, mines + e.mines
                    end
                end
            end
        end
        if sc >= TN_CELL_MIN then
            local px, py = c.sx // c.n, c.sy // c.n
            local dn = d2(px, py, bx, by)
            for _, o in ipairs(w.own) do
                dn = math.min(dn, d2(px, py, o[1], o[2]))
            end
            local ok = dn >= 32 * 32 and not within(w.drop, px, py, 18) and not within(w.foe, px, py, 22)
            ok = ok and not bad_spot(mem, px, py, now, 3) and (ai.danger(px, py) or 0) == 0
            if ok then
                local df = 0
                if #w.foe > 0 then
                    df = isqrt(nearest(w.foe, px, py))
                end
                if not (front >= 0 and df < front - TN_FRONT_STEP) then
                    local key = isqrt(dn) * 4 - math.min(sc, 24) - math.min(df, 160)
                    cand[#cand + 1] = {x = px, y = py, mines = mines, key = key, compact = dn <= TN_COMPACT * TN_COMPACT, ix = c.ix}
                end
            end
        end
    end
    -- within reach of a town first, else the nearest anywhere; with care the first whose spot and way are safe
    local care = S.towns_care and 1 or 0
    local tries = has(care, 3) and TN_CARE_TRIES or 1
    for _ = 1, tries do
        local bj = nil
        for j, c in ipairs(cand) do
            if not c.done and (bj == nil or (c.compact and not cand[bj].compact)
                    or (c.compact == cand[bj].compact and (c.key < cand[bj].key or (c.key == cand[bj].key and c.ix < cand[bj].ix)))) then
                bj = j
            end
        end
        if bj == nil then
            return nil
        end
        cand[bj].done = true
        if not has(care, 3) or tn_safe(view, w, cand[bj].x, cand[bj].y, cand[bj].mines, cand[bj].compact) then
            return {x = cand[bj].x, y = cand[bj].y}, cand[bj].mines
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- A town's citizens and buildings

-- a citizen that may go: not building, not aboard, on the home continent, not another town's (tn_free_cit; the
-- miners cannot be told apart here: a deviation)
local function taken_set(mem)
    local set = {}
    for _, t in ipairs(mem.towns) do
        if t.state ~= 0 then
            for _, id in ipairs(t.cit) do
                set[id] = true
            end
        end
    end
    if mem.ex ~= nil then
        for _, id in ipairs(mem.ex.pass or {}) do
            set[id] = true
        end
    end
    return set
end

-- the town's citizens up to n: the nearest free ones, held by the script
local function take_cits(mem, view, t, n)
    local set = taken_set(mem)
    local ids = ai.units{category = "citizen", held = "engine", near = {t.x, t.y, 300}, limit = 48}
    local cols = ai.get(ids, {"x", "y", "action", "inside"})
    local took = {}
    for k = 1, #ids do
        if #t.cit >= math.min(n, 12) then
            break
        end
        local id = ids[k]
        if cols.x[k] ~= nil and cols.inside[k] == nil and cols.action[k] ~= "build" and not set[id]
                and ai.reach(t.x, t.y, cols.x[k], cols.y[k]) then
            t.cit[#t.cit + 1] = id
            took[#took + 1] = id
        end
    end
    if #took > 0 then
        ai.take(took)
    end
    return #took
end

-- a forward base's production: the building of the soldier army_plan last picked, else Barracks
local function prod_type(memory)
    local a = memory.army or {}
    local p = a.plan ~= nil and a.plan.producer or a.last_producer
    return p or "b_barracks"
end

local function kind_type(view, kind, want)
    if kind == TK_PROD then
        return can_pay(view, want or "") and want or nil
    end
    if kind == TK_TOWER then
        local tc = tower_class(view)
        return tc ~= nil and can_pay(view, tc) and tc or nil
    end
    if kind == TK_HOUSE then
        return can_pay(view, "b_house") and "b_house" or nil
    end
    if not has(S.towns, 2) and can_pay(view, "b_town_center") then
        return "b_town_center"
    end
    return can_pay(view, "b_settlement") and "b_settlement" or nil
end

-- a spot for type near c on the home continent (tn_spot: the engine's rings, the safe ones first)
local function tn_spot(view, type, cx, cy)
    local x, y = ai.find_site(type, cx, cy, {min = 1, max = 10, safe = true})
    if x == nil then
        x, y = ai.find_site(type, cx, cy, {min = 1, max = 10})
    end
    if x ~= nil and ai.reach(view.town_x, view.town_y, x, y) then
        return x, y
    end
    return nil
end

-- the unit vector (thousandths) from a to b; nil when they are one
local function unit_vec(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    local l = isqrt(dx * dx + dy * dy)
    if l < 1 then
        return nil
    end
    return dx * 1000 // l, dy * 1000 // l
end

-- the nth tower's spot: 5 tiles from d0 towards the enemy it knows (else round it), to either side in turn
local function tower_spot(t, d0x, d0y, nth)
    local ux, uy = unit_vec(d0x, d0y, t.tx, t.ty)
    local rot = ROT
    if ux == nil then
        ux, uy, rot = 1000, 0, ROT_WIDE
    end
    local k = math.min(nth // 2 + 1, #rot)
    local c, s = rot[k][1], rot[k][2]
    if nth % 2 == 0 then
        s = -s
    end
    local rx = (ux * c - uy * s) // 1000
    local ry = (ux * s + uy * c) // 1000
    return d0x + round_div(5 * rx, 1000), d0y + round_div(5 * ry, 1000)
end

local function mark_bad(mem, x, y, now, loss, again)
    local b = nil
    for _, o in ipairs(mem.bad) do
        if d2(o.x, o.y, x, y) < 12 * 12 then
            b = o
        end
    end
    if b == nil then
        if #mem.bad >= 16 then
            table.remove(mem.bad, 1)
        end
        b = {x = x, y = y, losses = 0, again = 0}
        mem.bad[#mem.bad + 1] = b
    end
    if loss then
        b.losses = b.losses + 1
    end
    if again > b.again then
        b.again = again
    end
    return b.losses
end

local function release_cits(t)
    if #t.cit > 0 then
        ai.release(t.cit)
    end
    t.cit = {}
    t.mine = {}
end

local function tn_lost(mem, t, now)
    local n = mark_bad(mem, t.x, t.y, now, true, now + 480000)
    mem.lost = (mem.lost or 0) + 1
    mem.lost_t = now
    release_cits(t)
    say("lost at=%d,%d times=%d", t.x, t.y, n)
    t.state = 0
end

-- the forward base's wall (tn_wall): once its two front towers stand, 11 tiles straight across the enemy's side, 3
-- tiles in front of them (defence.c's front wall); a resource town 60+ tiles out with SPREAD too
local function tn_wall(mem, view, t, now)
    local far = has(S.towns, 16) and d2(t.x, t.y, view.town_x, view.town_y) >= TN_FAR_WALL * TN_FAR_WALL
    if not (t.forward or far) or t.wall_tries >= 3 or (t.wall_t ~= 0 and now - t.wall_t < 180000) or ai.wall == nil then
        return
    end
    local a0, a1, front, n = nil, nil, 0, 0
    for _, p in ipairs(t.proj) do
        if p.kind == TK_TOWER and n < 2 then
            n = n + 1
            if p.state == 2 then
                front = front + 1
                if a0 == nil then
                    a0 = p
                else
                    a1 = p
                end
            end
        end
    end
    if front < 2 then
        return
    end
    local wtype = wall_class(view)
    if wtype == nil then
        return
    end
    local wc = cost_of(view, wtype)
    for _, r in ipairs(V.resources) do
        if view.stock[r] < 11 * wc[r] then
            return
        end
    end
    local ux, uy = unit_vec(t.x, t.y, t.tx, t.ty)
    if ux == nil then
        return
    end
    local mx, my = (a0.x + a1.x) // 2, (a0.y + a1.y) // 2
    local cx, cy = mx + round_div(3 * ux, 1000), my + round_div(3 * uy, 1000)
    local px, py = -uy, ux
    local ax, ay = math.abs(px), math.abs(py)
    local sx = ax * 1000 > 2414 * ay and (px > 0 and 1 or -1) or (ay * 1000 > 2414 * ax and 0 or (px > 0 and 1 or -1))
    local sy = ay * 1000 > 2414 * ax and (py > 0 and 1 or -1) or (ax * 1000 > 2414 * ay and 0 or (py > 0 and 1 or -1))
    local h = (sx ~= 0 and sy ~= 0) and 4 or 5
    local x1, y1, x2, y2 = cx + sx * h, cy + sy * h, cx - sx * h, cy - sy * h
    t.wall_t = now
    t.wall_tries = t.wall_tries + 1
    if raiders_near(cx, cy, 10) > 0 or raiders_near(x1, y1, 10) > 0 or raiders_near(x2, y2, 10) > 0 then
        return
    end
    local map = ai.map()
    for _, q in ipairs({{x1, y1}, {x2, y2}, {cx, cy}}) do
        if q[1] < 3 or q[2] < 3 or q[1] >= map.width - 3 or q[2] >= map.height - 3 then
            return
        end
        local tile = ai.tile(q[1], q[2])
        if tile == nil or (tile.continent or 0) == 0 then
            return
        end
    end
    local builders = {}
    for _, id in ipairs(t.cit) do
        builders[#builders + 1] = id
    end
    if #builders == 0 then
        return
    end
    local code = ai.wall(builders, wtype, x1, y1, x2, y2)
    if code == "ok" then
        mem.walls = (mem.walls or 0) + 1
        say("wall type=%s from=%d,%d to=%d,%d town=%d,%d", wtype, x1, y1, x2, y2, t.x, t.y)
    end
end

local function tn_found(mem, memory, view, cx, cy, nmines, w, now, forward, fx, fy)
    if #mem.towns >= 128 then
        return nil
    end
    local t = {x = cx, y = cy, tx = cx, ty = cy, state = 1, forward = forward, wall_tries = 0, wall_t = 0, proj = {},
               cit = {}, mine = {}, t = now, plan_t = 0, hurt_t = 0, nmines = nmines}
    local _, f = nearest(w.foe, cx, cy)
    if f ~= nil then
        t.tx, t.ty = f[1], f[2]
    end
    local function proj(kind, want)
        t.proj[#t.proj + 1] = {kind = kind, want = want, state = 0, tries = 0, waits = 0, id = 0, x = 0, y = 0, type = ""}
    end
    if forward then   -- towers first (the cover), the Hospital, production, the flanks
        t.tx, t.ty = fx, fy
        proj(TK_TOWER)
        proj(TK_TOWER)
        if V.cost(view, "b_hospital") ~= nil then
            proj(TK_PROD, "b_hospital")
        end
        proj(TK_PROD, prod_type(memory))
        proj(TK_TOWER)
        proj(TK_TOWER)
    else
        proj(TK_DROP)
        -- SPREAD: 2 towers and 1 more for every 4 mines of its group, and 5 houses
        local towers = has(S.towns, 16) and 2 + nmines // 4 or S.towers_per_town
        towers = math.max(towers, S.towers_per_town)
        for _ = 1, towers do
            if #t.proj < TN_PROJ - 5 then
                proj(TK_TOWER)
            end
        end
        if has(S.towns, 16) then
            for _ = 1, TN_HOUSES do
                proj(TK_HOUSE)
            end
        end
    end
    take_cits(mem, view, t, forward and S.towns_citizens + 2 or S.towns_citizens)
    mem.founded = (mem.founded or 0) + 1
    -- the first free slot of the list (a lost town's record is reused)
    local slot = #mem.towns + 1
    for k, o in ipairs(mem.towns) do
        if o.state == 0 and slot > #mem.towns then
            slot = k
        end
    end
    mem.towns[slot] = t
    say("%s at=%d,%d mines=%d dist=%d citizens=%d", forward and "forward_base" or "resource_town", cx, cy, nmines,
        dist(cx, cy, view.town_x, view.town_y), #t.cit)
    return t
end

-- a town (tn_run, every 2 s): its buildings (laid in order, laid again when destroyed), its builders, its citizens
-- to its mines
local function tn_run(mem, memory, view, t, now)
    local standing, drop_ok, any, dropped = 0, false, false, 0
    local tries_max = 3
    for _, p in ipairs(t.proj) do
        if p.state == 1 or p.state == 2 then
            local u = ai.unit(p.id)
            if u == nil then
                -- a foundation gone within 10 s was never built (its builders could not reach it: the engine cancels
                -- it): no try spent, 6 such at most
                if p.state == 1 and now - (p.laid_t or 0) < 10000 and (p.vanished or 0) < 6 then
                    p.tries = p.tries - 1
                    p.vanished = (p.vanished or 0) + 1
                end
                say("%s destroyed at=%d,%d town=%d,%d again=%s", K_NAME[p.kind], p.x, p.y, t.x, t.y, tostring(p.tries < tries_max))
                p.id = 0
                p.state = p.tries < tries_max and 0 or 3
                t.hurt_t = now
                if p.state == 0 then
                    mem.rebuilt = (mem.rebuilt or 0) + 1
                end
            else
                if p.state == 1 and u.built then
                    p.state = 2
                    mem.built = (mem.built or 0) + 1
                end
                any = true
            end
        end
        if p.state == 2 then
            standing = standing + 1
        end
        if p.state == 3 then
            dropped = dropped + 1
        end
        if p.kind == TK_DROP and p.state == 2 then
            drop_ok = true
        end
    end
    if t.forward then
        drop_ok = true   -- none: its buildings go up at once
    end
    -- its citizens alive
    local cols = ai.get(t.cit, {"x", "y", "action", "tx", "ty", "inside"})
    local alive = {}
    for k, id in ipairs(t.cit) do
        if cols.x[k] ~= nil then
            alive[#alive + 1] = {id = id, x = cols.x[k], y = cols.y[k], action = cols.action[k], tx = cols.tx[k], ty = cols.ty[k], inside = cols.inside[k]}
        else
            t.hurt_t = now
            t.mine[id] = nil
        end
    end
    t.cit = {}
    for _, c in ipairs(alive) do
        t.cit[#t.cit + 1] = c.id
    end
    if not any and #t.cit == 0 and now - t.t > 60000 then
        tn_lost(mem, t, now)
        return
    end
    -- towns_remove_unbuilt: going up with all its citizens dead - its foundations removed and the town lost
    if S.towns_remove_unbuilt and t.state == 1 and any and #t.cit == 0 and now - t.t > 60000 then
        local removed = 0
        for _, p in ipairs(t.proj) do
            if p.state == 1 then
                ai.delete(p.id)
                p.id, p.state, removed = 0, 3, removed + 1
            end
        end
        say("citizens dead at=%d,%d removed=%d", t.x, t.y, removed)
        tn_lost(mem, t, now)
        return
    end
    -- towns_care parts 8 and 16 (off upstream: TN_CARE): hold under attack, give up a town going up that they hurt
    local care = S.towns_care and 1 or 0
    local hot = has(care, 8) and not t.forward and raiders_near(t.x, t.y, 12) > 0
    if hot and has(care, 16) and t.state == 1 and not drop_ok and t.hurt_t ~= 0 and now - t.hurt_t < 30000 then
        for _, p in ipairs(t.proj) do
            if p.state == 1 then
                ai.delete(p.id)
                p.id, p.state = 0, 3
            end
        end
        mem.gaveup = (mem.gaveup or 0) + 1
        say("given_up at=%d,%d why=hurt", t.x, t.y)
        tn_lost(mem, t, now)
        return
    end
    -- its first drop site not paid in 3 minutes: given up, not lost (that spot not again for 5 minutes)
    if t.proj[1].state == 0 and t.proj[1].tries == 0 and now - t.t > 180000 then
        mem.unpaid = (mem.unpaid or 0) + 1
        say("given_up at=%d,%d why=unpaid", t.x, t.y)
        mark_bad(mem, t.x, t.y, now, false, now + 300000)
        release_cits(t)
        t.state = 0
        return
    end
    tn_wall(mem, view, t, now)
    if t.state == 1 and t.forward and standing + dropped == #t.proj then
        release_cits(t)   -- its citizens back to the game
    end
    if t.state == 1 and drop_ok and standing + dropped == #t.proj then
        t.state = 2
        mem.held = (mem.held or 0) + 1
        say("stands at=%d,%d buildings=%d citizens=%d", t.x, t.y, standing, #t.cit)
    end
    if hot then
        return
    end
    -- the builders on each project (a citizen building whose goal lies by a project's tile)
    local on = {}
    local free = {}
    for _, c in ipairs(alive) do
        local j = nil
        if c.action == "build" and c.tx ~= nil then
            for q, p in ipairs(t.proj) do
                if p.state == 1 and math.abs(p.x - c.tx) <= 3 and math.abs(p.y - c.ty) <= 3 then
                    j = q
                end
            end
        end
        if j ~= nil then
            on[j] = on[j] or {}
            on[j][#on[j] + 1] = c
        else
            free[#free + 1] = c
        end
    end
    -- the next building (every 8 s), the drop site alone first; one it can't pay waits (the drop site always)
    if #t.cit > 0 and (t.plan_t == 0 or now - t.plan_t >= 8000) then
        t.plan_t = now
        local going = 0
        for _, p in ipairs(t.proj) do
            if p.state == 1 then
                going = going + 1
            end
        end
        for j, p in ipairs(t.proj) do
            if going >= 2 then
                break
            end
            if p.state == 0 then
                if p.kind ~= TK_DROP and not drop_ok and t.proj[1].state == 0 then
                    break
                end
                local ty = kind_type(view, p.kind, p.want)
                if ty == nil then
                    if p.kind ~= TK_DROP then
                        p.waits = p.waits + 1
                        if p.waits > 30 then
                            p.state = 3
                            say("dropped kind=%s town=%d,%d why=unpaid_4min", K_NAME[p.kind], t.x, t.y)
                        end
                    end
                    break
                end
                local cx, cy = t.x, t.y
                if p.kind == TK_PROD and not t.forward and t.proj[1].state ~= 0 then
                    cx, cy = t.proj[1].x, t.proj[1].y
                end
                if p.kind == TK_PROD and t.forward then   -- behind the towers
                    local ux, uy = unit_vec(t.x, t.y, t.tx, t.ty)
                    if ux ~= nil then
                        cx, cy = t.x - round_div(4 * ux, 1000), t.y - round_div(4 * uy, 1000)
                    end
                end
                if p.kind == TK_TOWER then
                    local d0x, d0y = t.x, t.y
                    if not t.forward and t.proj[1].state ~= 0 then
                        d0x, d0y = t.proj[1].x, t.proj[1].y
                    end
                    local nth = 0
                    for q = 1, j - 1 do
                        if t.proj[q].kind == TK_TOWER then
                            nth = nth + 1
                        end
                    end
                    cx, cy = tower_spot(t, d0x, d0y, nth)
                end
                local ax, ay = tn_spot(view, ty, cx, cy)
                -- the builders: all its free citizens for the drop site, else 3 (4 at a forward base) of them
                local builders = {}
                local most = p.kind == TK_DROP and 64 or (t.forward and 4 or 3)
                for _, c in ipairs(free) do
                    if #builders < most and c.inside == nil then
                        builders[#builders + 1] = c.id
                    end
                end
                if ax == nil or #builders == 0 then
                    if p.kind ~= TK_DROP then
                        p.waits = p.waits + 1
                        if p.waits > 30 then
                            p.state = 3
                        end
                    end
                    break
                end
                local code, reason, n, id = ai.build(builders, ty, ax, ay)
                if code ~= "ok" or id == nil then
                    if p.kind ~= TK_DROP then
                        p.waits = p.waits + 1
                        if p.waits > 30 then
                            p.state = 3
                        end
                    end
                    break
                end
                p.id, p.type, p.x, p.y, p.state, p.tries, p.laid_t = id, ty, ax, ay, 1, p.tries + 1, now
                mem.laid = (mem.laid or 0) + 1
                say("laid kind=%s type=%s at=%d,%d town=%d,%d builders=%d", K_NAME[p.kind], ty, ax, ay, t.x, t.y, #builders)
                going = going + 1
                local rest = {}
                for _, c in ipairs(free) do
                    local used = false
                    for _, b in ipairs(builders) do
                        if b == c.id then
                            used = true
                        end
                    end
                    if not used then
                        rest[#rest + 1] = c
                    end
                end
                free = rest
                on[j] = {}
                for _, b in ipairs(builders) do
                    on[j][#on[j] + 1] = {id = b}
                end
                if p.kind == TK_DROP then
                    break
                end
            end
        end
    end
    -- builders: all on the drop site again it stands, then 3 on each other going up; the rest gather by the town
    for j, p in ipairs(t.proj) do
        if p.state == 1 then
            local want = p.kind == TK_DROP and (drop_ok and 3 or 64) or (t.forward and 4 or 3)
            local have = on[j] ~= nil and #on[j] or 0
            local send = {}
            local rest = {}
            for _, c in ipairs(free) do
                if have + #send < want and c.inside == nil and (c.action ~= "gather" or not drop_ok or p.kind == TK_PROD) then
                    send[#send + 1] = c.id
                else
                    rest[#rest + 1] = c
                end
            end
            if #send > 0 then
                ai.repair(send, p.id)
                mem.orders = (mem.orders or 0) + 1
            end
            free = rest
        end
    end
    local function walk_to(c, x, y)
        if d2(c.x, c.y, x, y) > 64 and not (c.tx ~= nil and d2(c.tx, c.ty, x, y) <= 16) then
            ai.move(c.id, x, y)
            mem.sent = (mem.sent or 0) + 1
        end
    end
    local mines, mine_k = nil, 0
    for _, c in ipairs(free) do
        if t.forward then   -- to the base while nothing is laid; once it stands the game's again
            if t.state == 1 and not any then
                walk_to(c, t.x, t.y)
            end
        elseif not drop_ok then   -- no drop site yet and nothing to build: to the town
            if t.proj[1].state == 0 then
                walk_to(c, t.x, t.y)
            end
        elseif not t.mine[c.id] or c.action == "idle" then
            -- the town's mines: the free mines within 14 tiles of its middle, nearest first (one list a pass, the
            -- citizens spread over it); none - back to the game
            if mines == nil then
                mines = {}
                for _, kind in ipairs(MINERALS) do
                    local ids = ai.resources{kind = kind, near = {t.x, t.y, 14}, has_room = true, limit = 4}
                    local mc = ai.get(ids, {"x", "y", "amount"})
                    for k = 1, #ids do
                        if (mc.amount[k] or 0) > 0 then
                            mines[#mines + 1] = {id = ids[k], d = d2(mc.x[k], mc.y[k], t.x, t.y)}
                        end
                    end
                end
                table.sort(mines, function(a, b)
                    if a.d ~= b.d then
                        return a.d < b.d
                    end
                    return a.id < b.id
                end)
            end
            local best = nil
            if #mines > 0 then
                best = mines[mine_k % #mines + 1].id
                mine_k = mine_k + 1
            end
            if best ~= nil then
                ai.gather(c.id, best)
                t.mine[c.id] = true
                mem.assigned = (mem.assigned or 0) + 1
            elseif c.action == "idle" then
                ai.release(c.id)
                t.mine[c.id] = nil
                local keep = {}
                for _, id in ipairs(t.cit) do
                    if id ~= c.id then
                        keep[#keep + 1] = id
                    end
                end
                t.cit = keep
            end
        end
    end
end

-- its biggest group of land soldiers out in the field (an 8-tile cell 32+ tiles from the base), pulled back towards
-- the base out of the enemy's reach (tn_army_spot): the spot, the group's middle as the front, the size
local function army_spot(view)
    local bx, by = view.town_x, view.town_y
    local ids = ai.units{category = "soldier"}
    local cols = ai.get(ids, {"x", "y", "inside"})
    local cells = {}
    for k = 1, #ids do
        local x, y = cols.x[k], cols.y[k]
        if x ~= nil and cols.inside[k] == nil and d2(x, y, bx, by) >= 32 * 32 and ai.reach(bx, by, x, y) then
            local key = (x // 8) * 1000 + y // 8
            local c = cells[key] or {n = 0, sx = 0, sy = 0}
            c.n, c.sx, c.sy = c.n + 1, c.sx + x, c.sy + y
            cells[key] = c
        end
    end
    local best = nil
    for _, key in ipairs(ai.keys(cells)) do
        local c = cells[key]
        if best == nil or c.n > best.n then
            best = c
        end
    end
    if best == nil or best.n < 8 then
        return nil
    end
    local mx, my = best.sx // best.n, best.sy // best.n
    local len = dist(mx, my, bx, by)
    for d = 0, len - 32 do
        local qx, qy = mx + (bx - mx) * d // math.max(len, 1), my + (by - my) * d // math.max(len, 1)
        local tile = ai.tile(qx, qy)
        if tile ~= nil and (tile.continent or 0) ~= 0 and ai.reach(bx, by, qx, qy) and not stage_danger(qx, qy) then
            return qx, qy, mx, my, best.n
        end
    end
    return nil
end

-- SPREAD's pace: citizens to spare (towns_citizens idle or beyond their resource's target) and twice a Town Center's
-- price in stock (tn_surplus)
local function surplus(view, budget)
    local idle = view.idle
    local target = budget.target or {}
    for _, r in ipairs(V.resources) do
        idle = idle + math.max((view.gatherers[r] or 0) - (target[r] or 0), 0)
    end
    if idle < S.towns_citizens then
        return false
    end
    local tc = cost_of(view, "b_town_center")
    for _, r in ipairs(V.resources) do
        if view.stock[r] < 2 * tc[r] then
            return false
        end
    end
    return true
end

local function tn_plan(mem, memory, view, budget, hc, now)
    local n, going, nf, fgoing = 0, 0, 0, 0
    for _, t in ipairs(mem.towns) do
        if t.forward then
            nf = nf + (t.state ~= 0 and 1 or 0)
            fgoing = fgoing + (t.state == 1 and 1 or 0)
        else
            n = n + (t.state ~= 0 and 1 or 0)
            going = going + (t.state == 1 and 1 or 0)
        end
    end
    local why = nil
    if view.epoch < FARM_EPOCH then
        why = "before_copper"
    elseif view.citizens < 20 then
        why = "fewer_than_20_workers"
    elseif view.threat_strength > 0 then
        why = "enemies_in_base"
    end
    local w = nil
    if why == nil then
        w = world(mem, view)
        local done = false
        local out, mine, theirs = outmatched(view)
        if has(S.towns, 8) then   -- a forward base where the army gathers, under its cover
            local tc = tower_class(view)
            local rich = tc ~= nil and can_pay(view, tc)
            local cx, cy, fx, fy, army = nil, nil, nil, nil, 0
            local am = memory.army or {}
            local force = require("lib.groups").role(memory, "force")
            if (am.stage_x or -1) >= 0 and force ~= nil and am.out then
                local cols = ai.get(force.units, {"x", "y"})
                local at = 0
                for k = 1, #force.units do
                    if cols.x[k] ~= nil and d2(cols.x[k], cols.y[k], am.stage_x, am.stage_y) <= 12 * 12 then
                        at = at + 1
                    end
                end
                if at >= 8 then
                    cx, cy, fx, fy, army = am.stage_x, am.stage_y, force.x or am.stage_x, force.y or am.stage_y, at
                end
            end
            if army < 8 then
                cx, cy, fx, fy, army = army_spot(view)
                army = army or 0
            end
            local clear = true
            if army > 0 then
                clear = d2(cx, cy, view.town_x, view.town_y) >= 32 * 32 and not within(w.own, cx, cy, 32)
                for _, t in ipairs(mem.towns) do
                    if t.state ~= 0 and t.forward and d2(cx, cy, t.x, t.y) < 32 * 32 then
                        clear = false
                    end
                end
            end
            if fgoing > 0 then
                why = "forward_base_going_up"
            elseif nf >= 2 then
                why = "two_forward_bases"
            elseif army < 8 then
                why = "no_group_of_8_in_the_field"
            elseif not clear then
                why = "army_within_32_of_a_town"
            elseif mine * 10 < theirs * 8 then
                why = "army_under_0.8"
            elseif not rich then
                why = "saving_for_forward_base"
                if (mem.fsave or 0) == 0 or now - mem.fsave > 240000 then
                    mem.fsave = now   -- 3 minutes of saving, then a minute's pause
                end
            else
                mem.plans = (mem.plans or 0) + 1
                mem.fsave = 0
                tn_found(mem, memory, view, cx, cy, 0, w, now, true, fx, fy)
                done, why = true, nil
            end
            if why ~= nil and why ~= "saving_for_forward_base" then
                mem.fsave = 0
            end
        end
        if not done and has(S.towns, 1) then
            why = nil
            local spread = has(S.towns, 16)
            local most = (spread and view.citizens >= 2 * TN_SPREAD_BUILDING) and view.citizens // TN_SPREAD_BUILDING or 1
            local care = S.towns_care and 1 or 0
            if has(care, 4) and most > 2 then
                most = 2
            end
            if going >= most then
                why = "town_going_up"
            elseif has(care, 4) and (mem.lost_t or 0) ~= 0 and now - mem.lost_t < TN_CARE_PAUSE then
                why = "town_lost_recently"
            elseif not spread and n >= S.towns_max then
                why = "towns_max"
            elseif spread and view.citizens < TN_SPREAD_WORKERS * (n + 2) then
                why = "too_few_workers"
            elseif spread and not surplus(view, budget) then
                why = "no_surplus"
            elseif out then
                why = "army_outmatched"
            end
            if why == nil then
                local fs = free_slots(view)
                if fs >= 4 and not spread then
                    why = "free_places_at_its_mines"
                else
                    local c, nm
                    if spread then
                        c, nm = tn_site_spread(mem, view, hc, w, now)
                    else
                        c, nm = tn_site(mem, view, hc, w, now)
                    end
                    if c == nil then
                        why = "nothing_free_to_settle"
                    else
                        mem.plans = (mem.plans or 0) + 1
                        tn_found(mem, memory, view, c.x, c.y, nm or 0, w, now, false)
                    end
                end
            end
        end
    end
    if why ~= nil and why ~= mem.why and now - (mem.why_t or 0) >= 60000 then
        mem.why, mem.why_t = why, now
        say("no_new_town why=%s towns=%d going=%d", why, n + nf, going + fgoing)
    elseif why == nil then
        mem.why = nil
    end
end

-- the resources the towns wait for (towns_reserve), into the economy's reserve: a town's first drop site (a Town
-- Center for 90 s, then a Settlement, 3 minutes at most), its next tower once the drop site is laid, a forward
-- base's two towers and Hospital while it saves
local function towns_reserve(mem, view, now, reserve)
    if (mem.fsave or 0) ~= 0 and now - mem.fsave < 180000 then
        local tc = tower_class(view) or "b_guard_tower_middle"
        local tw, ho = cost_of(view, tc), cost_of(view, "b_hospital")
        for _, r in ipairs(V.resources) do
            reserve[r] = reserve[r] + 2 * tw[r] + ho[r]
        end
    end
    for _, t in ipairs(mem.towns) do
        if t.state == 1 and not t.forward then
            local first = t.proj[1]
            if first.state == 0 and first.tries == 0 and now - t.t <= 180000 then
                local ty = (now - t.t < 90000 and not has(S.towns, 2) and V.cost(view, "b_town_center") ~= nil) and "b_town_center" or "b_settlement"
                local c = cost_of(view, ty)
                for _, r in ipairs(V.resources) do
                    reserve[r] = reserve[r] + c[r]
                end
            elseif first.state ~= 0 then
                local pending = false
                for j = 2, #t.proj do
                    if t.proj[j].state == 0 then
                        pending = true
                    end
                end
                if pending then
                    local c = cost_of(view, tower_class(view) or "b_guard_tower_middle")
                    for _, r in ipairs(V.resources) do
                        reserve[r] = reserve[r] + c[r]
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- EXPANSION TO OTHER ISLANDS (expand.c)

local function ex_bits()
    return S.expand_islands or 0
end

local function ex_guard()
    return (has(ex_bits(), 128) and S.expand_guard < 4) and 4 or S.expand_guard
end

local function ex_losses()
    return has(ex_bits(), 256) and 2 or 3
end

-- the land continents and what the player knows of them (ex_scan): the explored mines, its own buildings, citizens
-- and soldiers, the allies' buildings seen, the enemy buildings, defences and towns it knows. Keyed by land id
local function ex_scan(mem, view)
    local x = mem.ex
    x.land = x.land or {}   -- the land of a building or a mine, by id (they do not move)
    local map = ai.map()
    local far = map.width + map.height
    local bx, by = view.town_x, view.town_y
    local conts = {}
    local function cont_of(id)
        local c = conts[id]
        if c == nil then
            c = {id = id, mn = 0, msx = 0, msy = 0, x = -1, y = -1, bld = 0, cit = 0, sold = 0, ally = 0, eseen = 0, etow = 0, etown = 0}
            conts[id] = c
        end
        return c
    end
    local function land_of(id, ux, uy)
        local l = x.land[id]
        if l == nil then
            l = land_near(ux, uy)
            if l ~= 0 then
                x.land[id] = l
            end
        end
        return l
    end
    for _, kind in ipairs(MINERALS) do
        local ids = ai.resources{kind = kind, near = {bx, by, far}}
        local cols = ai.get(ids, {"x", "y", "amount"})
        for k = 1, #ids do
            if (cols.amount[k] or 0) > 0 then
                local l = land_of(ids[k], cols.x[k], cols.y[k])
                if l ~= 0 then
                    local c = cont_of(l)
                    c.mn, c.msx, c.msy = c.mn + 1, c.msx + cols.x[k], c.msy + cols.y[k]
                    c.x, c.y = cols.x[k], cols.y[k]
                end
            end
        end
    end
    local ids = ai.units{category = "building"}
    local cols = ai.get(ids, {"x", "y", "type"})
    for k = 1, #ids do
        if cols.x[k] ~= nil and cols.type[k] ~= nil and not cols.type[k]:find("wall", 1, true) then
            local l = land_of(ids[k], cols.x[k], cols.y[k])
            if l ~= 0 then
                cont_of(l).bld = cont_of(l).bld + 1
            end
        end
    end
    x.cls = x.cls or {}
    for _, m in ipairs(ai.memory{buildings = true}) do
        local l = land_of(m.id, m.x, m.y)
        if l ~= 0 then
            local c = cont_of(l)
            c.eseen = c.eseen + 1
            if x.cls[m.type] == nil then
                local cls = ai.class(m.type)
                x.cls[m.type] = (cls ~= nil and (cls.attack or 0) > 0) and 1 or 0
            end
            c.etow = c.etow + x.cls[m.type]
            if TOWN_TYPES[m.type] then
                c.etown = c.etown + 1
            end
        end
    end
    ids = ai.seen{enemy = false, category = "building"}
    cols = ai.get(ids, {"x", "y"})
    for k = 1, #ids do
        if cols.x[k] ~= nil then
            local l = land_of(ids[k], cols.x[k], cols.y[k])
            if l ~= 0 then
                cont_of(l).ally = cont_of(l).ally + 1
            end
        end
    end
    return conts
end

-- the free island for a new outpost (ex_pick); nil and why
local function ex_pick(mem, view, conts, hc, now)
    local x = mem.ex
    local bits = ex_bits()
    local best, bs, any = nil, nil, false
    local bx, by = view.town_x, view.town_y
    x.scouted = x.scouted or {}
    for _, id in ipairs(ai.keys(conts)) do
        local c = conts[id]
        -- an island with 2+ mines it has explored; or one with a mine seen and land it has not looked at yet, which
        -- the ferry's transport scouts first (phase 4; upstream knew every mine from the zones and scouted the
        -- islands it had hardly looked at)
        local known = c.mn >= 2 or (c.mn >= 1 and not x.scouted[id] and c.x >= 0 and ai.unexplored(c.x, c.y, {land = id, grid = 4}) ~= nil)
        local skip = id == hc or not known or (has(bits, 32) and c.bld > 0)
        if not skip then
            any = true
            local gone = false
            for _, g in ipairs(x.gone) do
                if g.cont == id and (g.lost >= ex_losses() or now < g.again) then
                    gone = true
                end
            end
            if not (c.etown > 0 or (c.ally > 0 and not has(bits, 4)) or c.etow > 0 or c.eseen > 3 or gone) then
                local mx, my = c.msx // c.mn, c.msy // c.mn
                if raiders_known(mx, my, 20) <= ex_guard() then
                    local sx = ai.shore(mx, my, {land = id, max = 25})
                    if sx ~= nil then
                        local d = dist(mx, my, bx, by)
                        if has(bits, 32) then   -- MORE: concentrated - the nearest of what it holds
                            for _, h in ipairs(x.held) do
                                d = math.min(d, dist(mx, my, h.x, h.y))
                            end
                        end
                        local sc = 40 * c.mn - d - 60 * c.eseen
                        if bs == nil or sc > bs or (sc == bs and id < best) then
                            best, bs = id, sc
                        end
                    end
                end
            end
        end
    end
    local why = (not any) and "no_other_island_with_2_mines" or (best == nil and "every_island_held_defended_or_lost") or nil
    return best, why
end

local function ex_mark_lost(mem, cont, now)
    local x = mem.ex
    for _, g in ipairs(x.gone) do
        if g.cont == cont then
            g.lost = g.lost + 1
            g.again = now + (has(ex_bits(), 256) and 900000 or 480000)
            return g.lost
        end
    end
    if #x.gone >= 8 then
        return 0
    end
    x.gone[#x.gone + 1] = {cont = cont, lost = 1, again = now + (has(ex_bits(), 256) and 900000 or 480000)}
    return 1
end

-- the ferry's end: the passengers still at home are free again, a transport with someone aboard unloads at the land
-- nearest to it
local function ex_ferry_end(mem, view, now, why)
    local x = mem.ex
    local ship = x.ship ~= 0 and ai.unit(x.ship) or nil
    local home = {}
    for _, id in ipairs(x.pass) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil then
            home[#home + 1] = id
        end
    end
    if #home > 0 then
        ai.release(home)
    end
    if ship ~= nil and (ship.cargo or 0) > 0 then
        local land = x.fphase == 3 and x.cont or x.hc
        local tx, ty = ai.shore(ship.x, ship.y, {sea = x.sea, land = land, max = 30})
        if tx == nil then
            tx, ty = ai.shore(ship.x, ship.y, {sea = x.sea, land = x.hc, max = 40})
        end
        if tx ~= nil then
            ai.unload(x.ship, tx, ty)
        end
    end
    if ship ~= nil then
        ai.release(x.ship)
    end
    if why ~= nil then
        say("ferry_end why=%s island=%d", why, x.cont)
    end
    x.guard_only = false
    x.fphase, x.pass, x.ship = 0, {}, 0
end

-- the sea on the way (ex_sea_risk, simplified): the enemy warships it knows near the way against its own warships,
-- in percent of the strength
local function ex_sea_risk(mem, view)
    local x = mem.ex
    local mx, my = (x.home_wx + x.unload_wx) // 2, (x.home_wy + x.unload_wy) // 2
    local r = dist(x.home_wx, x.home_wy, x.unload_wx, x.unload_wy) // 2 + 20
    local theirs = 0
    for _, m in ipairs(ai.memory{near = {mx, my, r}, since = view.time - 600000}) do
        if not m.building then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                local t = ai.tile(m.x, m.y)
                if t ~= nil and (t.sea or 0) ~= 0 then
                    theirs = theirs + m.hp * c.attack
                end
            end
        end
    end
    local own = ai.strength(ai.units{category = "ship", armed = true})
    return theirs * 100 // (theirs + own + 1)
end

-- phase 1: a transport (its own, idle and empty, the last trip's first, else trained), then the passengers
local function ex_ferry_ship(mem, memory, view, order, budget, now)
    local x = mem.ex
    local bits = ex_bits()
    local landing = memory.navy ~= nil and memory.navy.landing or nil
    local best, bd = nil, nil
    for _, tr in ipairs(N.transports()) do
        local taken = false
        if landing ~= nil then
            for _, id in ipairs(landing.ships or {}) do
                if id == tr.id then
                    taken = true
                end
            end
        end
        if not taken and tr.cargo == 0 then
            local t = ai.tile(tr.x, tr.y)
            local sea = t ~= nil and t.sea or 0
            if sea == 0 then
                local _, _, s2 = ai.shore(tr.x, tr.y, {sea = x.sea, max = 3})
                sea = s2 or 0
            end
            if sea == x.sea then
                local d = d2(tr.x, tr.y, x.home_wx, x.home_wy)
                if tr.id == x.last then
                    d = -1
                end
                if bd == nil or d < bd then
                    best, bd = tr, d
                end
            end
        end
    end
    if best == nil then
        local wait = has(bits, 8) and 720000 or 360000
        if now - x.fsince >= wait and ((x.ftrained_t or 0) == 0 or now - x.ftrained_t >= 120000) then
            ex_ferry_end(mem, view, now, "no_transport")
            x.next_try = now + 600000
            if not x.ever_ashore then
                x.active = false
            end
            return
        end
        if now >= (x.ftrain_t or 0) then
            x.ftrain_t = now + 30000
            local tr = nil
            for _, c in ipairs(view.costs) do
                if not c.building and c.producer == "b_dock" then
                    local cls = ai.class(c.type)
                    if cls ~= nil and (cls.transport or 0) > 0 and (tr == nil or c.type > tr.type) then
                        tr = c
                    end
                end
            end
            if tr ~= nil and tr.requested + tr.in_production == 0 then
                order({verb = "train", type = tr.type, n = 1})
                x.trained = (x.trained or 0) + 1
                x.ftrained_t = now
                say("transport ordered type=%s island=%d", tr.type, x.cont)
            end
        end
        return
    end
    x.ship = best.id
    x.scouted = x.scouted or {}
    if not x.scouted[x.cont] then   -- an island it has hardly looked at: scouted first, empty
        local conts = ex_scan(mem, view)
        local c = conts[x.cont]
        if c == nil or c.mn < 2 or ai.unexplored(x.mines_x, x.mines_y, {land = x.cont, grid = 4}) ~= nil then
            local risk = ex_sea_risk(mem, view)
            if risk > 40 then   -- as the ferry's crossing
                say("wont_scout island=%d risk=%d", x.cont, risk)
                x.ship, x.fphase, x.active, x.next_try = 0, 0, false, now + 180000
                return
            end
            ai.take(x.ship)
            x.fphase, x.fsince, x.scout_at, x.scout_skips, x.sg_t = 4, now, 0, {}, 0
            x.scouts = (x.scouts or 0) + 1
            ai.move(x.ship, x.unload_wx, x.unload_wy, {forced = true})
            say("scouts island=%d mines_known=%d ship=%d to=%d,%d", x.cont, c ~= nil and c.mn or 0, x.ship, x.unload_wx, x.unload_wy)
            return
        end
    end
    -- the passengers: citizens nearest the home shore (within 35 tiles; builders last, the idle first with FOLLOW),
    -- soldiers at home (the free ones)
    local set = taken_set(mem)
    local cap = best.cap
    local guard = x.guard_trip and math.max(ex_guard() - x.nsold, 0) or 0
    if has(bits, 256) then
        local want = ex_guard()
        if x.ncit // 3 > want then
            want = x.ncit // 3
        end
        guard = x.guard_trip and math.max(want - x.nsold, 0) or 0
    end
    guard = math.min(guard, cap)
    local cits = x.guard_only and 0 or (x.ever_ashore and cap - guard or S.expand_citizens)
    cits = math.min(cits, cap - guard, S.expand_citizens + 4)
    if x.ever_ashore then
        cits = math.min(cits, x.slots - x.ncit)
    end
    local cand = {}
    local ids = ai.units{category = "citizen", held = "engine", near = {x.home_x, x.home_y, 35}, limit = 48}
    local cols = ai.get(ids, {"x", "y", "action", "inside"})
    for k = 1, #ids do
        if cols.x[k] ~= nil and cols.inside[k] == nil and not set[ids[k]] and ai.reach(x.home_x, x.home_y, cols.x[k], cols.y[k]) then
            local d = d2(cols.x[k], cols.y[k], x.home_x, x.home_y)
            if cols.action[k] == "build" then
                d = d + 400
            end
            if cols.action[k] == "idle" and has(bits, 128) then
                d = d - 2600
            end
            cand[#cand + 1] = {id = ids[k], d = d}
        end
    end
    table.sort(cand, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.id < b.id
    end)
    x.pass = {}
    for k = 1, math.min(cits, #cand) do
        x.pass[#x.pass + 1] = cand[k].id
    end
    local nc = #x.pass
    if guard > 0 then
        local sids = ai.units{category = "soldier", held = "engine", near = {x.home_x, x.home_y, 60}, limit = 24}
        local sc = ai.get(sids, {"x", "y", "inside", "action"})
        local n = 0
        for k = 1, #sids do
            if n < guard and sc.x[k] ~= nil and sc.inside[k] == nil and sc.action[k] ~= "attack"
                    and ai.reach(x.home_x, x.home_y, sc.x[k], sc.y[k]) then
                x.pass[#x.pass + 1] = sids[k]
                n = n + 1
                x.guard[#x.guard + 1] = sids[k]   -- the outpost's own guard from now on
            end
        end
        -- the free ones first, then the force's (armies.c ar_take: soldiers at home, never those out on a push)
        local force = require("lib.groups").role(memory, "force")
        local am = memory.army or {}
        if n < guard and force ~= nil and not am.out then
            local fc = ai.get(force.units, {"x", "y", "inside"})
            local cand = {}
            for k, id in ipairs(force.units) do
                if fc.x[k] ~= nil and fc.inside[k] == nil and ai.reach(x.home_x, x.home_y, fc.x[k], fc.y[k]) then
                    cand[#cand + 1] = {id = id, d = d2(fc.x[k], fc.y[k], x.home_x, x.home_y)}
                end
            end
            table.sort(cand, function(a, b)
                if a.d ~= b.d then
                    return a.d < b.d
                end
                return a.id < b.id
            end)
            local taken = {}
            for k = 1, math.min(guard - n, #cand) do
                x.pass[#x.pass + 1] = cand[k].id
                x.guard[#x.guard + 1] = cand[k].id
                taken[cand[k].id] = true
            end
            local keep = {}
            for _, id in ipairs(force.units) do
                if not taken[id] then
                    keep[#keep + 1] = id
                end
            end
            force.units = keep
        end
    end
    if (x.guard_only and #x.pass == 0) or (not x.guard_only and nc < 3) then
        ex_ferry_end(mem, view, now, x.guard_only and "no_soldiers_near_shore" or "fewer_than_3_citizens_near_shore")
        x.next_try = now + 120000
        if not x.ever_ashore then
            x.active = false
        end
        return
    end
    ai.take(x.pass)
    ai.take(x.ship)
    x.fphase, x.fsince, x.freissue, x.load_t = 2, now, 0, 0
    say("boarding citizens=%d soldiers=%d at=%d,%d island=%d mines=%d,%d unload=%d,%d ship=%d", nc, #x.pass - nc, x.home_x, x.home_y,
        x.cont, x.mines_x, x.mines_y, x.unload_x, x.unload_y, x.ship)
end

-- phase 4: the empty transport looks at the island (ex_ferry_scout): an enemy town, defence or 4+ known buildings -
-- that island never again; looked at - back to the home shore for the passengers (phase 1 again); 4 minutes: given
-- up. Upstream waits 10 s at the shore by the mines (it knew the mines from the zones); here the transport sails
-- along the island's shore from one unexplored grid point to the next (ai.unexplored) until none is left
local function ex_ferry_scout(mem, view, now)
    local x = mem.ex
    x.scouted = x.scouted or {}
    local ship = ai.unit(x.ship)
    local function home(why, again, looked)
        ai.move(x.ship, x.home_wx, x.home_wy, {forced = true})
        if looked then
            x.scouted[x.cont] = true
        end
        say("scouting_end island=%d why=%s", x.cont, why)
        x.fphase, x.pass, x.ship, x.active, x.next_try = 0, {}, 0, false, now + again
        ai.release(ship.id)
    end
    if now - (x.scan4_t or 0) >= 10000 then
        x.scan4_t = now
        local conts = ex_scan(mem, view)
        local c = conts[x.cont]
        if c ~= nil and (c.etown > 0 or c.etow > 0 or c.eseen > 3) then
            while ex_mark_lost(mem, x.cont, now) < 3 and #x.gone < 8 do
            end
            x.scout_bad = (x.scout_bad or 0) + 1
            home(c.etown > 0 and "enemy_town" or c.etow > 0 and "enemy_defences" or "enemy_buildings", 30000, true)
            return
        end
        x.scout_mn = c ~= nil and c.mn or 0
    end
    local gx, gy = ai.unexplored(x.mines_x, x.mines_y, {land = x.cont, grid = 4, skip = x.scout_skips})
    -- and upstream's look: 10 s at the shore water by the mines (it sees the shore and what stands by it)
    if d2(ship.x, ship.y, x.unload_wx, x.unload_wy) <= 36 then
        if x.scout_at == 0 then
            x.scout_at = now
        end
    elseif gx == nil then
        x.scout_at = 0
        if now - (x.sg_t or 0) >= 20000 then
            x.sg_t = now
            ai.move(x.ship, x.unload_wx, x.unload_wy, {forced = true})
        end
        return
    end
    if gx == nil and (x.scout_at == 0 or now - x.scout_at < 10000) then
        return
    end
    if gx == nil then   -- looked at: the passengers next, when the island is worth it
        x.scouted[x.cont] = true
        if (x.scout_mn or 0) < 2 then
            home("fewer_than_2_mines", 30000, true)
            return
        end
        ai.move(x.ship, x.home_wx, x.home_wy, {forced = true})
        x.fphase, x.fsince = 1, now
        say("scouted island=%d mines=%d - the passengers next", x.cont, x.scout_mn)
        return
    end
    if now - x.fsince >= 240000 then
        home("not_looked_at_in_4min", 180000, true)
        return
    end
    -- the next shore point: the goal reached (or 30 s on it) is skipped
    local reached = x.sg_t ~= 0 and (d2(ship.x, ship.y, x.sg_x, x.sg_y) <= 36 or now - x.sg_t >= 30000)
    if x.sg_t == 0 or reached then
        if reached and #x.scout_skips < 32 then
            x.scout_skips[#x.scout_skips + 1] = {x.sg_gx, x.sg_gy}
            gx, gy = ai.unexplored(x.mines_x, x.mines_y, {land = x.cont, grid = 4, skip = x.scout_skips})
            if gx == nil then
                return
            end
        end
        local sx, sy = ai.shore(gx, gy, {sea = x.sea, land = x.cont, max = 12})
        local wx, wy = nil, nil
        if sx ~= nil then
            wx, wy = water_by(sx, sy, x.sea)
        end
        if wx == nil then   -- no shore of its sea by that point: skipped
            if #x.scout_skips < 32 then
                x.scout_skips[#x.scout_skips + 1] = {gx, gy}
            end
            return
        end
        x.sg_x, x.sg_y, x.sg_gx, x.sg_gy, x.sg_t = wx, wy, gx, gy, now
        ai.move(x.ship, wx, wy, {forced = true})
    end
end

-- phase 2: to the home shore, aboard, then off
local function ex_ferry_board(mem, view, now)
    local x = mem.ex
    local ship = ai.unit(x.ship)
    local cols = ai.get(x.pass, {"x", "y", "inside", "category", "tx", "ty"})
    local pass, aboard, cits = {}, 0, 0
    for k, id in ipairs(x.pass) do
        if cols.category[k] ~= nil then
            pass[#pass + 1] = id
            if cols.inside[k] ~= nil then
                aboard = aboard + 1
                if cols.category[k] == "citizen" then
                    cits = cits + 1
                end
            end
        end
    end
    local there = d2(ship.x, ship.y, x.home_wx, x.home_wy) <= 49
    if not there and x.load_t == 0 and now - (x.ship_t or 0) >= 20000 then
        x.ship_t = now
        ai.move(x.ship, x.home_wx, x.home_wy, {forced = true})
    end
    local all = aboard == #pass and #pass > 0
    if all or (x.load_t ~= 0 and now - x.load_t >= 90000 and aboard * 10 >= #pass * 7)
            or (now - x.fsince >= 240000 and (cits >= 3 or (x.guard_only and aboard > 0))) then
        if cits < 3 and not x.guard_only then
            x.pass = pass
            ex_ferry_end(mem, view, now, "boarding_failed")
            x.next_try = now + 300000
            if not x.ever_ashore then
                x.active = false
            end
            return
        end
        local risk = ex_sea_risk(mem, view)
        if risk > 40 then   -- the sea held: unloaded at home at once
            x.pass = pass
            ex_ferry_end(mem, view, now, string.format("sea_held risk=%d", risk))
            x.next_try = now + 180000
            if not x.ever_ashore then
                x.active = false
            end
            return
        end
        local left, keep = {}, {}
        for k, id in ipairs(x.pass) do
            if cols.category[k] ~= nil then
                if cols.inside[k] ~= nil then
                    keep[#keep + 1] = id
                else
                    left[#left + 1] = id
                end
            end
        end
        if #left > 0 then
            ai.release(left)   -- those left at home are free again
        end
        x.pass = keep
        ai.unload(x.ship, x.unload_x, x.unload_y)
        x.fphase, x.fsince, x.unload_t = 3, now, now
        x.sails = (x.sails or 0) + 1
        say("sails aboard=%d citizens=%d to=%d,%d island=%d", aboard, cits, x.unload_x, x.unload_y, x.cont)
        return
    end
    x.pass = pass
    if now - x.fsince >= 300000 then
        ex_ferry_end(mem, view, now, "boarding_failed_5min")
        x.next_try = now + 300000
        if not x.ever_ashore then
            x.active = false
        end
        return
    end
    -- the passengers to the shore first; the transport there and 3 in 4 near, or 2 minutes: the load (again every
    -- 20 s for those not aboard)
    local close, walk = 0, {}
    cols = ai.get(x.pass, {"x", "y", "inside", "tx", "ty"})   -- the live passengers' columns
    for k = 1, #x.pass do
        if cols.inside[k] ~= nil or (cols.x[k] ~= nil and d2(cols.x[k], cols.y[k], x.home_x, x.home_y) <= 64) then
            close = close + 1
        elseif cols.x[k] ~= nil and x.load_t == 0 and not (cols.tx[k] ~= nil and d2(cols.tx[k], cols.ty[k], x.home_x, x.home_y) <= 16) then
            walk[#walk + 1] = x.pass[k]
        end
    end
    if #walk > 0 then
        ai.move(walk, x.home_x, x.home_y)
    end
    if x.load_t == 0 and not ((there and close * 4 >= #x.pass * 3) or now - x.fsince >= 120000) then
        return
    end
    if x.freissue ~= 0 and now - x.freissue < 20000 then
        return
    end
    x.freissue = now
    if x.load_t == 0 then
        x.load_t = now
    end
    local list = {}
    for k = 1, #x.pass do
        if cols.inside[k] == nil and cols.x[k] ~= nil then
            list[#list + 1] = x.pass[k]
        end
    end
    if #list > 0 then
        ai.load(list, x.ship)
        x.orders = (x.orders or 0) + 1
    end
end

-- phase 3: at sea - unloaded again where it stopped with someone aboard; all ashore: the transport back, the ferry
-- ends
local function ex_ferry_sail(mem, view, now)
    local x = mem.ex
    local ship = ai.unit(x.ship)
    if (ship.cargo or 0) <= 0 then
        local ashore = 0
        for _, id in ipairs(x.pass) do
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil then
                ashore = ashore + 1
            end
        end
        x.ashore = (x.ashore or 0) + 1
        x.ever_ashore = true
        x.empty_t = 0
        if #x.proj > 0 and (x.base_t or 0) == 0 then
            x.base_t = now
        end
        ai.move(x.ship, x.home_wx, x.home_wy, {forced = true})
        ai.release(x.ship)
        x.last = x.ship
        x.fphase, x.pass, x.ship, x.trip_t, x.guard_only = 0, {}, 0, now, false
        x.plan_t = now   -- the first building 8 s on: a foundation whose builders are still landing is cancelled
        say("ashore island=%d landed=%d", x.cont, ashore)
        return
    end
    if now - x.fsince >= 360000 then
        ex_ferry_end(mem, view, now, "no_landing_in_6min")
        x.next_try = now + 300000
        if not x.ever_ashore then
            x.active = false
        end
        return
    end
    if ship.action == "unload" or now - (x.unload_t or 0) < 2000 then
        return
    end
    local tx, ty = x.unload_x, x.unload_y
    if d2(ship.x, ship.y, x.unload_x, x.unload_y) <= 144 then   -- off the island: the nearest shore tile with room
        local sx, sy = ai.shore(ship.x, ship.y, {sea = x.sea, land = x.cont, max = 12})
        if sx ~= nil then
            tx, ty = sx, sy
        end
    end
    x.unload_t = now
    x.retries = (x.retries or 0) + 1
    ai.unload(x.ship, tx, ty)
end

local function xk_type(view, kind)
    if kind == XK_TOWER then
        local tc = tower_class(view)
        return tc ~= nil and can_pay(view, tc) and tc or nil
    end
    if kind == XK_BARRACKS then
        return can_pay(view, "b_barracks") and "b_barracks" or nil
    end
    return can_pay(view, "b_settlement") and "b_settlement" or nil   -- a Settlement (a Town Center first took the stone of the towers)
end

local function ex_spot(x, type, cx, cy)
    local sx, sy = ai.find_site(type, cx, cy, {min = 1, max = 12, safe = true})
    if sx == nil then
        sx, sy = ai.find_site(type, cx, cy, {min = 1, max = 12})
    end
    if sx ~= nil and ai.reach(x.mines_x, x.mines_y, sx, sy) then
        return sx, sy
    end
    return nil
end

-- the free mine of the island nearest to p
local function ex_free_mine(x, px, py)
    local best, bd = nil, nil
    for _, kind in ipairs(MINERALS) do
        local ids = ai.resources{kind = kind, near = {px, py, 30}, has_room = true, limit = 4}
        local cols = ai.get(ids, {"x", "y", "amount"})
        for k = 1, #ids do
            if (cols.amount[k] or 0) > 0 and ai.reach(x.mines_x, x.mines_y, cols.x[k], cols.y[k]) then
                local d = d2(cols.x[k], cols.y[k], px, py)
                if bd == nil or d < bd then
                    best, bd = ids[k], d
                end
            end
        end
    end
    return best
end

-- SEA CONTROL as the AI knows it (sea_held): no enemy warship seen in its own waters (40 tiles of its dock) within
-- 5 minutes, and its warships at least the enemy's it saw within 10 minutes
local function sea_held(view, now)
    local _, dock = N.dock_sea()
    local n, foe = 0, 0
    for _, m in ipairs(ai.memory{since = now - 600000}) do
        if not m.building then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                local t = ai.tile(m.x, m.y)
                if t ~= nil and (t.sea or 0) ~= 0 then
                    n = n + 1
                    if dock ~= nil and now - m.seen <= 300000 and d2(m.x, m.y, dock.x, dock.y) <= 40 * 40 then
                        foe = foe + 1
                    end
                end
            end
        end
    end
    local own = #ai.units{category = "ship", armed = true}
    return foe == 0 and own >= n, own, n, foe
end

-- FORWARD: the outpost nearer the enemy's known buildings than its home - a barracks and two more towers there,
-- while it holds the sea (once)
local function ex_forward(mem, view, now)
    local x = mem.ex
    if x.fwd_added then
        return
    end
    if not x.forward then
        local dh, dp = nil, nil
        for _, m in ipairs(ai.memory{buildings = true}) do
            local a, b = d2(m.x, m.y, view.town_x, view.town_y), d2(m.x, m.y, x.mines_x, x.mines_y)
            dh = (dh == nil or a < dh) and a or dh
            dp = (dp == nil or b < dp) and b or dp
        end
        if dp == nil or dp >= dh then
            return
        end
        x.forward = true
        x.forwards = (x.forwards or 0) + 1
        say("forward_outpost island=%d dist=%d home=%d", x.cont, isqrt(dp), isqrt(dh))
    end
    local held, own, n, inw = sea_held(view, now)
    if not held then
        if (x.sea_no or 0) == 0 or now - x.sea_no >= 300000 then
            x.sea_no = now
            say("forward_waits why=sea_not_held own=%d theirs=%d in_waters=%d", own, n, inw)
        end
        return
    end
    for _, k in ipairs({XK_BARRACKS, XK_TOWER, XK_TOWER}) do
        if #x.proj < EX_PROJ and not (k == XK_BARRACKS and has(ex_bits(), 2)) then
            x.proj[#x.proj + 1] = {kind = k, state = 0, tries = 0, waits = 0, id = 0, x = 0, y = 0, type = ""}
        end
    end
    x.fwd_added = true
    say("forward_base island=%d own=%d theirs=%d", x.cont, own, n)
end

-- MORE: the outpost holds - its Settlement and towers stand (or were dropped), no enemy there for 3 minutes, its
-- trips made or its slots filled: left to its own gatherers (held_tick), the next island planned
local function ex_holds(mem, view, now)
    local x = mem.ex
    for _, p in ipairs(x.proj) do
        if p.state ~= 2 and p.state ~= 3 then
            return false
        end
    end
    if ((x.foe_t or 0) ~= 0 and now - x.foe_t < 180000) or x.fphase ~= 0 or x.ncit < 3
            or (x.trips < S.expand_trips and x.ncit + 3 <= x.slots) then
        return false
    end
    if #x.held >= 4 then
        return false
    end
    local ax, ay = x.mines_x, x.mines_y
    for _, p in ipairs(x.proj) do
        if p.kind == XK_DROP and p.state == 2 then
            ax, ay = p.x, p.y
        end
    end
    x.held[#x.held + 1] = {cont = x.cont, x = ax, y = ay}
    x.active = false
    x.graduated = (x.graduated or 0) + 1
    x.next_try = now + 60000
    x.guard, x.forward, x.fwd_added = {}, false, false
    say("holds island=%d citizens=%d soldiers=%d laid=%d held=%d", x.cont, x.ncit, x.nsold, #x.proj, #x.held)
    return true
end

-- the own units on an island near a point: citizens, soldiers (held ones first), buildings, the enemy seen within 14
local function ex_isle(x, mx, my, take_soldiers)
    local e = {c = {}, s = {}, nb = 0, foe = 0, fu = nil, fd = nil}
    local guard = {}   -- the outpost's own soldiers (never a landing's: lib.groups holds those)
    for _, id in ipairs(x.guard) do
        guard[id] = true
    end
    local ids = ai.units{category = "citizen", near = {mx, my, 40}}
    local cols = ai.get(ids, {"x", "y", "action", "inside", "tx", "ty", "held"})
    for k = 1, #ids do
        if cols.x[k] ~= nil and cols.inside[k] == nil and ai.reach(mx, my, cols.x[k], cols.y[k]) then
            e.c[#e.c + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], action = cols.action[k], tx = cols.tx[k], ty = cols.ty[k], held = cols.held[k]}
        end
    end
    ids = ai.units{category = "soldier", near = {mx, my, 40}}
    cols = ai.get(ids, {"x", "y", "action", "inside", "held"})
    local take = {}
    for k = 1, #ids do
        if cols.x[k] ~= nil and cols.inside[k] == nil and ai.reach(mx, my, cols.x[k], cols.y[k]) then
            local own = guard[ids[k]] == true
            if not own and take_soldiers and cols.held[k] == "engine" then
                take[#take + 1] = ids[k]
                own = true
            end
            if own then
                e.s[#e.s + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], action = cols.action[k]}
            end
        end
    end
    if #take > 0 then
        ai.take(take)
    end
    local live = ai.get(x.guard, {"x"})   -- the dead dropped from the guard's list (those aboard a ferry stay)
    local keep = {}
    for k, id in ipairs(x.guard) do
        if live.x[k] ~= nil then
            keep[#keep + 1] = id
        end
    end
    for _, id in ipairs(take) do
        keep[#keep + 1] = id
    end
    x.guard = keep
    ids = ai.units{category = "building", near = {mx, my, 40}}
    cols = ai.get(ids, {"x", "y"})
    for k = 1, #ids do
        if cols.x[k] ~= nil and ai.reach(mx, my, cols.x[k], cols.y[k]) then
            e.nb = e.nb + 1
        end
    end
    ids = ai.seen{enemy = true, near = {mx, my, 14}}
    cols = ai.get(ids, {"x", "y", "category"})
    for k = 1, #ids do
        local c = cols.category[k]
        if (c == "soldier" or c == "citizen" or c == "hero") and ai.reach(mx, my, cols.x[k], cols.y[k]) then
            e.foe = e.foe + 1
            local d = d2(cols.x[k], cols.y[k], mx, my)
            if e.fd == nil or d < e.fd then
                e.fu, e.fd = ids[k], d
            end
        end
    end
    return e
end

-- THE OUTPOST'S WORK (ex_outpost, every 2 s): the projects, builders, miners and the guard
local function ex_outpost(mem, view, now)
    local x = mem.ex
    local bits = ex_bits()
    local mx, my = x.mines_x, x.mines_y   -- the outpost's middle: its drop site once laid
    for _, p in ipairs(x.proj) do
        if p.kind == XK_DROP and p.state ~= 0 and p.state ~= 3 then
            mx, my = p.x, p.y
        end
    end
    local e = ex_isle(x, mx, my, not has(bits, 64))
    x.ncit, x.nsold = #e.c, #e.s
    if e.foe > 0 then
        x.foe_t = now
    end
    -- lost: nothing of it on the island any more (and no ferry on its way)
    if x.ever_ashore and #e.c == 0 and e.nb == 0 and x.fphase ~= 3 then
        if (x.empty_t or 0) == 0 then
            x.empty_t = now
        end
        if now - x.empty_t >= 20000 then
            local n = ex_mark_lost(mem, x.cont, now)
            x.lost = (x.lost or 0) + 1
            x.active = false
            if x.fphase ~= 0 then
                ex_ferry_end(mem, view, now, nil)
            end
            say("outpost_lost island=%d times=%d given_up=%s", x.cont, n, tostring(n >= ex_losses()))
            if n >= ex_losses() then
                x.given = (x.given or 0) + 1
            end
            x.next_try = now + 60000
        end
        return
    end
    x.empty_t = 0
    if #e.c == 0 and #e.s == 0 then
        return
    end
    -- the projects: going up -> standing or destroyed; standing -> destroyed: laid again (3 times)
    local drop_ok = false
    for _, p in ipairs(x.proj) do
        if p.state == 1 or p.state == 2 then
            local u = ai.unit(p.id)
            if u == nil then
                -- a foundation gone within 10 s was never built (its builders could not reach it: the engine cancels
                -- it): no try spent, 6 such at most
                if p.state == 1 and now - (p.laid_t or 0) < 10000 and (p.vanished or 0) < 6 then
                    p.tries = p.tries - 1
                    p.vanished = (p.vanished or 0) + 1
                end
                local again = p.tries < (has(bits, 256) and 8 or 3)
                say("%s destroyed at=%d,%d island=%d again=%s", XK_NAME[p.kind], p.x, p.y, x.cont, tostring(again))
                p.id, p.state = 0, again and 0 or 3
            elseif p.state == 1 and u.built then
                p.state = 2
                if p.kind == XK_DROP then
                    x.drops = (x.drops or 0) + 1
                end
                x.built = (x.built or 0) + 1
                say("%s finished at=%d,%d island=%d", XK_NAME[p.kind], p.x, p.y, x.cont)
            end
        end
        if p.kind == XK_DROP and p.state == 2 then
            drop_ok = true
        end
    end
    x.drop_ok = drop_ok
    if drop_ok and has(bits, 64) then
        ex_forward(mem, view, now)
    end
    if drop_ok and has(bits, 32) and ex_holds(mem, view, now) then
        return
    end
    do   -- the drop site and the first tower laid: no more reserve for them
        local laid = 0
        for _, p in ipairs(x.proj) do
            if p.state ~= 0 and p.kind ~= XK_BARRACKS then
                laid = laid + 1
            end
        end
        x.res_t = laid >= 2 and 0 or now
    end
    -- the builders on each project
    local on, free = {}, {}
    for _, c in ipairs(e.c) do
        local j = nil
        if c.action == "build" and c.tx ~= nil then
            for q, p in ipairs(x.proj) do
                if p.state == 1 and math.abs(p.x - c.tx) <= 3 and math.abs(p.y - c.ty) <= 3 then
                    j = q
                end
            end
        end
        if j ~= nil then
            on[j] = on[j] or {}
            on[j][#on[j] + 1] = c
        else
            free[#free + 1] = c
        end
    end
    -- the next project (every 8 s): in order, with its spot; one it can't pay waits (the drop site always)
    if #e.c > 0 and ((x.plan_t or 0) == 0 or now - x.plan_t >= 8000) then
        x.plan_t = now
        local going = 0
        for _, p in ipairs(x.proj) do
            if p.state == 1 then
                going = going + 1
            end
        end
        for j, p in ipairs(x.proj) do
            if going >= 2 then
                break
            end
            if p.state == 0 then
                local ty = xk_type(view, p.kind)
                if ty == nil then
                    if p.kind ~= XK_DROP and not has(bits, 256) then
                        p.waits = p.waits + 1
                        if p.waits > 30 then
                            p.state = 3
                            say("dropped kind=%s island=%d why=unpaid_4min", XK_NAME[p.kind], x.cont)
                        end
                    end
                    break
                end
                local cx, cy = x.mines_x, x.mines_y
                if p.kind == XK_TOWER then   -- between the mines and the sea (the first), else by the drop site
                    local first = true
                    for q = 1, j - 1 do
                        if x.proj[q].kind == XK_TOWER then
                            first = false
                        end
                    end
                    if first then
                        cx, cy = (x.mines_x + x.unload_x) // 2, (x.mines_y + x.unload_y) // 2
                    else
                        cx, cy = mx, my
                    end
                elseif p.kind == XK_BARRACKS then
                    cx, cy = mx, my
                end
                local ax, ay = ex_spot(x, ty, cx, cy)
                local builders = {}
                local most = p.kind == XK_DROP and 64 or 2
                for _, c in ipairs(free) do
                    if #builders < most and c.action ~= "carried" then
                        builders[#builders + 1] = c.id
                    end
                end
                local code, id = nil, nil
                if ax ~= nil and #builders > 0 then
                    local reason, n
                    code, reason, n, id = ai.build(builders, ty, ax, ay)
                end
                if code ~= "ok" or id == nil then
                    if p.kind ~= XK_DROP then
                        p.waits = p.waits + 1
                        if p.waits > 30 then
                            p.state = 3
                        end
                    end
                    break
                end
                p.id, p.type, p.x, p.y, p.state, p.tries, p.laid_t = id, ty, ax, ay, 1, p.tries + 1, now
                say("laid kind=%s type=%s at=%d,%d island=%d citizens=%d soldiers=%d", XK_NAME[p.kind], ty, ax, ay, x.cont, #e.c, #e.s)
                going = going + 1
                local rest = {}
                for _, c in ipairs(free) do
                    local used = false
                    for _, b in ipairs(builders) do
                        if b == c.id then
                            used = true
                        end
                    end
                    if not used then
                        rest[#rest + 1] = c
                    end
                end
                free = rest
                on[j] = {}
                for _, b in ipairs(builders) do
                    on[j][#on[j] + 1] = {id = b}
                end
                if p.kind == XK_DROP then
                    break
                end
            end
        end
    end
    -- builders: 3 on the drop site (all while it is not finished), 2 on another; the rest gather once the drop site
    -- stands
    for j, p in ipairs(x.proj) do
        if p.state == 1 then
            local want = p.kind == XK_DROP and (drop_ok and 3 or 64) or 2
            local have = on[j] ~= nil and #on[j] or 0
            local send, rest = {}, {}
            for _, c in ipairs(free) do
                if have + #send < want and (c.action ~= "gather" or not drop_ok) then
                    send[#send + 1] = c.id
                else
                    rest[#rest + 1] = c
                end
            end
            if #send > 0 then
                ai.repair(send, p.id)
                x.orders = (x.orders or 0) + 1
            end
            free = rest
        end
    end
    local mine = nil
    for _, c in ipairs(free) do
        if drop_ok and c.action ~= "gather" and c.action ~= "build" and c.action ~= "move" then
            if mine == nil then
                mine = ex_free_mine(x, mx, my) or 0   -- one look a pass (the engine spreads the gatherers by room)
            end
            if mine ~= 0 then
                ai.gather(c.id, mine)
                x.assigned = (x.assigned or 0) + 1
            end
        end
    end
    -- the guard: by the drop site, attacking what it sees within 14 tiles
    local idle, fighters = {}, {}
    for _, s in ipairs(e.s) do
        if s.action ~= "attack" then
            if e.fu ~= nil then
                fighters[#fighters + 1] = s.id
            elseif d2(s.x, s.y, mx, my) > 64 and s.action ~= "move" then
                idle[#idle + 1] = s.id
            end
        end
    end
    if #fighters > 0 then
        ai.attack(fighters, e.fu)
        x.orders = (x.orders or 0) + 1
    end
    if #idle > 0 then
        ai.move(idle, mx, my + 2)
        x.orders = (x.orders or 0) + 1
    end
end

-- an enemy town (Town Center, Capitol) seen on the outpost's island - an enemy's home, not a free island
local function ex_enemy_home(mem, view, conts, now)
    local x = mem.ex
    local c = conts[x.cont]
    if c == nil or c.etown == 0 then
        return
    end
    while ex_mark_lost(mem, x.cont, now) < 3 and #x.gone < 8 do
    end
    if x.fphase == 3 then
        x.fphase = 2   -- unloaded at the home shore, not on that island
    end
    if x.fphase ~= 0 then
        ex_ferry_end(mem, view, now, nil)
    end
    x.active = false
    x.given = (x.given or 0) + 1
    x.next_try = now + 60000
    say("enemy_home island=%d", x.cont)
end

-- FOLLOW: why not now (nil: the sea held, the army 1.5 x the enemy's it knows)
local function ex_follow_no(view, now, mine, theirs)
    if not has(ex_bits(), 128) then
        return nil
    end
    if not sea_held(view, now) then
        return "sea_not_held"
    end
    if mine * 2 < theirs * 3 then
        return "army_not_1.5x"
    end
    return nil
end

-- a held island's gatherers (MORE): every 30 s its idle citizens to a free mine there
local function held_tick(mem, view, now)
    local x = mem.ex
    if #x.held == 0 or now - (x.held_t or 0) < 30000 then
        return
    end
    x.held_t = now
    for _, h in ipairs(x.held) do
        local ids = ai.units{category = "citizen", idle = true, near = {h.x, h.y, 30}, limit = 12}
        local cols = ai.get(ids, {"x", "y", "inside"})
        local tmp = {mines_x = h.x, mines_y = h.y}
        for k = 1, #ids do
            if cols.x[k] ~= nil and cols.inside[k] == nil and ai.reach(h.x, h.y, cols.x[k], cols.y[k]) then
                local s = ex_free_mine(tmp, h.x, h.y)
                if s ~= nil then
                    ai.gather(ids[k], s)
                end
            end
        end
    end
end

-- a new outpost (phase 0, every 30 s) or more citizens for it (ex_plan)
local function ex_plan(mem, memory, view, order, now)
    local x = mem.ex
    local bits = ex_bits()
    local why = nil
    local hc = x.hc
    if x.active then   -- more citizens: the drop site stands, slots left, trips left, not into a fight
        if not x.drop_ok or x.trips >= S.expand_trips or ((x.trip_t or 0) ~= 0 and now - x.trip_t < 120000)
                or ((x.foe_t or 0) ~= 0 and now - x.foe_t < 60000) or now - (x.more_t or 0) < 30000 then
            return
        end
        x.more_t = now   -- (the look every 30 s: upstream scans every tick)
        local conts = ex_scan(mem, view)
        x.slots = (conts[x.cont] ~= nil and conts[x.cont].mn or 0) * mine_cap(view)
        local out, mine, theirs = outmatched(view)
        if x.ncit + 3 > x.slots or out or ex_follow_no(view, now, mine, theirs) ~= nil then
            return
        end
        x.guard_trip = x.nsold < ex_guard()
        x.fphase, x.fsince, x.ftrain_t = 1, now, 0
        x.trips = x.trips + 1
        say("more_citizens island=%d there=%d slots=%d soldiers=%d trip=%d", x.cont, x.ncit, x.slots, x.nsold, x.trips)
        return
    end
    if view.town_x < 0 or hc == 0 then
        return
    end
    local conts = nil
    if has(bits, 32) then   -- MORE: a held island with nothing of it left is lost
        conts = ex_scan(mem, view)
        local keep = {}
        for _, h in ipairs(x.held) do
            local c = conts[h.cont]
            local cit = #ai.units{category = "citizen", near = {h.x, h.y, 40}, limit = 1}
            if (c ~= nil and c.bld > 0) or cit > 0 then
                keep[#keep + 1] = h
            else
                local n = ex_mark_lost(mem, h.cont, now)
                x.held_lost = (x.held_lost or 0) + 1
                say("held_lost island=%d times=%d", h.cont, n)
            end
        end
        x.held = keep
        if #x.held >= S.expand_max then
            why = "holds_expand_max"
        end
    end
    if why == nil then
        if not S.landing then
            why = "landing_off"
        elseif view.epoch < FARM_EPOCH or now < S.expand_minute * 60000 then
            why = "too_early"
        end
    end
    local mine, theirs = 0, 0
    if why == nil then
        conts = conts or ex_scan(mem, view)
        local h = conts[hc]
        local slots = (h ~= nil and h.mn or 0) * mine_cap(view)
        local miners = view.gatherers.stone + view.gatherers.gold + view.gatherers.iron
        local early = has(bits, 8)
        local out
        out, mine, theirs = outmatched(view)
        if not early and now < 900000 and miners * 2 < slots then
            why = "home_mines_first"
        elseif view.citizens < (early and 20 or 25) then
            why = early and "fewer_than_20_workers" or "fewer_than_25_workers"
        elseif out then
            why = "enemy_army_stronger"
        else
            why = ex_follow_no(view, now, mine, theirs)
        end
    end
    local sea = nil
    if why == nil then
        sea = N.dock_sea()
        if sea == nil then
            why = "no_dock"
        end
    end
    local cont, c, mx, my = nil, nil, 0, 0
    if why == nil then
        cont, why = ex_pick(mem, view, conts, hc, now)
        c = cont ~= nil and conts[cont] or nil
        if why == nil and c == nil then
            why = "no_island"
        end
    end
    local ux, uy, uwx, uwy = nil, nil, nil, nil
    if why == nil then
        mx, my = c.msx // c.mn, c.msy // c.mn
        ux, uy = ai.shore(mx, my, {sea = sea, land = cont, max = 30})
        if ux ~= nil then
            uwx, uwy = water_by(ux, uy, sea)
        end
        if ux == nil or uwx == nil then
            why = "no_shore_of_its_sea_on_island"
        end
    end
    local hx, hy, hwx, hwy = nil, nil, nil, nil
    if why == nil then   -- the home shore: near the base, towards the island
        local bx, by = view.town_x, view.town_y
        local best = nil
        local len = math.max(dist(bx, by, ux, uy), 1)
        for _, d in ipairs({0, 10, 20, 30}) do
            local px, py = bx + (ux - bx) * d // len, by + (uy - by) * d // len
            local sx, sy = ai.shore(px, py, {sea = sea, land = hc, max = 40})
            if sx ~= nil and d2(sx, sy, bx, by) <= 40 * 40 then
                local wx, wy = water_by(sx, sy, sea)
                if wx ~= nil then
                    local sc = dist(sx, sy, bx, by) * 2 + dist(sx, sy, ux, uy)
                    if best == nil or sc < best then
                        best, hx, hy, hwx, hwy = sc, sx, sy, wx, wy
                    end
                end
            end
        end
        if best == nil then
            why = "no_shore_of_its_sea_at_home"
        end
    end
    if why ~= nil then
        if why ~= x.why or (x.log_t or 0) == 0 or now - x.log_t >= 300000 then
            x.log_t = now
            local lands = {}
            for _, id in ipairs(ai.keys(conts or {})) do
                local c = conts[id]
                lands[#lands + 1] = string.format("%d:m%d/b%d/e%d", id, c.mn, c.bld, c.eseen)
            end
            say("none_planned why=%s home=%d lands=%s", why, hc, table.concat(lands, ","))
        end
        x.why = why
        return
    end
    x.why = nil
    x.active, x.cont, x.sea, x.mines_x, x.mines_y = true, cont, sea, mx, my
    x.unload_x, x.unload_y, x.unload_wx, x.unload_wy = ux, uy, uwx, uwy
    x.home_x, x.home_y, x.home_wx, x.home_wy = hx, hy, hwx, hwy
    x.ever_ashore, x.trips, x.drop_ok, x.empty_t, x.base_t, x.plan_t, x.ncit, x.nsold = false, 1, false, 0, 0, 0, 0, 0
    x.guard, x.forward, x.fwd_added, x.sea_no, x.guard_only, x.foe_t = {}, false, false, 0, false, 0
    x.slots = c.mn * mine_cap(view)
    x.proj = {}
    local plan = {XK_DROP, XK_TOWER, XK_TOWER}
    if has(bits, 128) or has(bits, 256) then
        plan = {XK_DROP, XK_TOWER, XK_TOWER, XK_TOWER, XK_TOWER}
    end
    if has(bits, 2) then
        plan[#plan + 1] = XK_BARRACKS
    end
    for _, k in ipairs(plan) do
        x.proj[#x.proj + 1] = {kind = k, state = 0, tries = 0, waits = 0, id = 0, x = 0, y = 0, type = ""}
    end
    x.guard_trip = true
    x.fphase, x.fsince, x.ftrain_t, x.res_t = 1, now, 0, now
    x.plans = (x.plans or 0) + 1
    say("plans_outpost island=%d mines=%d enemy_buildings=%d at=%d,%d unload=%d,%d board=%d,%d", cont, c.mn, c.eseen, mx, my, ux, uy, hx, hy)
end

-- the drop site and a tower an outpost waits for, and its transport (expand_hold): held back from the unit orders
local function expand_hold(mem, view, now, reserve)
    local x = mem.ex
    if x == nil or ex_bits() == 0 then
        return
    end
    -- (upstream holds the transport it waits for as well; here the engine's reserve would refuse the transport's own
    -- train order, so only the drop site and the tower are held)
    if (x.res_t or 0) == 0 or now - x.res_t > 180000 then
        return
    end
    local st, tw = cost_of(view, "b_settlement"), cost_of(view, tower_class(view) or "b_guard_tower_middle")
    for _, r in ipairs(V.resources) do
        reserve[r] = reserve[r] + st[r] + tw[r]
    end
end

local function expand_act(mem, memory, view, order, budget, now)
    local x = mem.ex
    if (x.t or 0) ~= 0 and now - x.t < 2000 then
        return
    end
    x.t = now
    if ex_bits() == 0 then
        if x.fphase ~= 0 then
            ex_ferry_end(mem, view, now, nil)
        end
        x.active = false
        return
    end
    x.hc = land_near(view.town_x, view.town_y)
    if x.fphase >= 2 and ai.unit(x.ship) == nil then
        say("transport_lost phase=%d island=%d", x.fphase, x.cont)
        local left = {}
        for _, id in ipairs(x.pass) do
            if ai.unit(id) ~= nil then
                left[#left + 1] = id
            end
        end
        if #left > 0 then
            ai.release(left)
        end
        x.fphase, x.pass, x.ship = 0, {}, 0
        x.next_try = now + 300000
        if not x.ever_ashore then
            ex_mark_lost(mem, x.cont, now)
            x.active = false
            x.lost = (x.lost or 0) + 1
        end
    end
    if x.active and now - (x.scan_t or 0) >= 30000 then
        x.scan_t = now
        ex_enemy_home(mem, view, ex_scan(mem, view), now)
    end
    if x.fphase == 1 then
        ex_ferry_ship(mem, memory, view, order, budget, now)
    elseif x.fphase == 2 then
        ex_ferry_board(mem, view, now)
    elseif x.fphase == 3 then
        ex_ferry_sail(mem, view, now)
    elseif x.fphase == 4 then
        ex_ferry_scout(mem, view, now)
    end
    if x.active and x.ever_ashore then
        ex_outpost(mem, view, now)
    end
    held_tick(mem, view, now)
    if x.fphase == 0 and now >= (x.next_try or 0) and ((x.plan_t or 0) == 0 or x.active or now - x.plan_t >= 30000) then
        if not x.active then
            x.plan_t = now
        end
        ex_plan(mem, memory, view, order, now)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step

-- whether a tile lies within r of a town of the script's (econ.lua's settlements keep off them; eco_mem.town_spots
-- is written by M.step)
-- [metal.c] calm_navy_cap reads the sea as this module knows it
M.sea_held = sea_held

function M.near_town(eco_mem, x, y, r)
    for _, t in ipairs(eco_mem.town_spots or {}) do
        if d2(t.x, t.y, x, y) < r * r then
            return true
        end
    end
    return false
end

function M.step(view, memory, orders, budget)
    memory.towns = memory.towns or {towns = {}, bad = {}, t = 0, plan_t = 0}
    local mem = memory.towns
    mem.towns = mem.towns or {}
    mem.bad = mem.bad or {}
    mem.ex = mem.ex or {active = false, fphase = 0, pass = {}, ship = 0, proj = {}, gone = {}, held = {}, guard = {}, trips = 0,
                        ncit = 0, nsold = 0, slots = 0, cont = 0, hc = 0, last = 0, next_try = 0, fsince = 0, t = 0}
    local now = view.time
    local function order(o)
        orders[#orders + 1] = o
    end
    -- the reserve the economy's next step holds for the towns and the outpost (towns_reserve, expand_reserve).
    -- [priority.c] the outpost row reads the two apart: the towns' (towns_reserve) and the outpost's (expand_hold:
    -- a building waits for it only with spend_tiers, a landing's order never)
    prio_memory = memory
    local reserve, ex_hold = {}, {}
    for _, r in ipairs(V.resources) do
        reserve[r] = 0
        ex_hold[r] = 0
    end
    if view.town_x >= 0 then
        expand_act(mem, memory, view, order, budget, now)
        if (S.towns or 0) ~= 0 then
            if mem.t == 0 or now - mem.t >= 2000 then
                mem.t = now
                local hc = land_near(view.town_x, view.town_y)
                if hc ~= 0 then
                    for _, t in ipairs(mem.towns) do
                        if t.state ~= 0 then
                            tn_run(mem, memory, view, t, now)
                        end
                    end
                    if mem.plan_t == 0 or now - mem.plan_t >= 30000 then
                        mem.plan_t = now
                        tn_plan(mem, memory, view, budget, hc, now)
                    end
                end
            end
            towns_reserve(mem, view, now, reserve)
        end
        expand_hold(mem, view, now, ex_hold)
    end
    memory.eco = memory.eco or {}
    memory.eco.towns_reserve = reserve
    memory.eco.expand_hold = ex_hold   -- [priority.c]
    local spots = {}
    for _, t in ipairs(mem.towns) do
        if t.state ~= 0 then
            spots[#spots + 1] = {x = t.x, y = t.y}
        end
    end
    if mem.ex.active then   -- the outpost's and the held islands' mines are theirs too
        spots[#spots + 1] = {x = mem.ex.mines_x, y = mem.ex.mines_y}
    end
    for _, h in ipairs(mem.ex.held) do
        spots[#spots + 1] = {x = h.x, y = h.y}
    end
    memory.eco.town_spots = spots
    if view.time % 300000 < 1000 and ((mem.plans or 0) > 0 or (mem.ex.plans or 0) > 0) then
        local n, held = 0, 0
        for _, t in ipairs(mem.towns) do
            if t.state ~= 0 then
                n = n + 1
            end
            if t.state == 2 then
                held = held + 1
            end
        end
        local x = mem.ex
        say("status towns=%d standing=%d planned=%d founded=%d stood=%d lost=%d unpaid=%d laid=%d built=%d again=%d walls=%d | outpost=%s island=%d ferry=%d cit=%d sold=%d plans=%d sails=%d ashore=%d drops=%d lost=%d held=%d",
            n, held, mem.plans or 0, mem.founded or 0, mem.held or 0, mem.lost or 0, mem.unpaid or 0, mem.laid or 0, mem.built or 0,
            mem.rebuilt or 0, mem.walls or 0, tostring(x.active), x.cont, x.fphase, x.ncit, x.nsold, x.plans or 0, x.sails or 0,
            x.ashore or 0, x.drops or 0, x.lost or 0, #x.held)
    end
end

return M

-- Age of AI port: upstream dll/ai/territories.c (the territories as an AI sees them: the foes and the guard of
-- each, UNSAFE with its calm timers, front and backland, the seas' claim, the guard wanted, the refuge, the border
-- towers and border walls; UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.territories").
--
-- THE MODEL (deviation, and the heart of this file): upstream reads game/territory.c, the retail engine's split of
-- the map into about 40 regions cut at the necks, and game/reachmap.c's areas. THIS engine exposes no territory
-- split and no region table, so the port builds its own:
--   the map in TC_CELL-tile cells, a cell land when 2+ of its 9 sampled tiles are on a land continent, water when
--     2+ are on a sea, "other" (a forest, a cliff, the map's edge) otherwise - and only EXPLORED tiles tell, so the
--     cells fill in as the AI looks;
--   a cell's land mass (and a sea) by union-find over the known cells of one class, 8 ways;
--   within each land mass a TERRITORY grown from seeds: the seeds are a lattice of one cell every TC_SEED (about
--     40 tiles), and a land mass the lattice does not reach is seeded when no grown territory reaches it either - a
--     multi-source BFS (the cell's distance from its seed) assigns every cell to the nearest seed, chunked over
--     steps (TC_GROW cells a step, the queue kept in the memory): a cheap watershed, stable - a cell's territory is
--     assigned once and never changes, so the model does not flicker between steps;
--   a sea is ONE territory per connected water body (upstream's water regions may split an engine sea and its
--     tv_seas then groups them by the engine's sea id, which here is the sea itself - a deviation, and the reason a
--     sea's claim covers its whole water body).
-- The boundaries are therefore NOT the retail game's: the retail split cuts at the land's necks and follows the map
-- generator's resources; the lattice cuts where the seeds' distances meet, so a long coast or a peninsula is cut
-- into cells of about 40 tiles. The DECISIONS built on them keep their intent (which ground is mine, where the
-- fight is, which ground is a front, which sea is mine to watch), which is what upstream reads its regions for.
-- Fog honesty: only explored cells exist, so a territory is what the AI has seen of its land, and the facts are
-- what it sees and remembers (own units always; enemies only through ai.memory - ai.seen for raiders and allies).
--
-- THE FACTS (every TV_EVERY per AI; upstream's tv_tick):
--   a territory's foes - enemy land soldiers in it or within TV_NEAR tiles of it (a fight knows no border: a drop
--        site 3 tiles across one was the first "refuge"); those seen now and those seen in the last TV_MEMORY;
--   its guard - its land soldiers in it, its citizens, and its defences standing in it;
--   UNSAFE - the guard loses (the ratio of hit points x attack under TV_LOSING; upstream's st_ratio needs the damage
--        tables this engine does not expose - a deviation), until it wins (TV_HOLD) or the foes are gone, and for
--        TV_CALM after the last look that said so. Only the territories with TV_FORCE foes and something of the AI's
--        in them are judged, TV_HOT a pass, rotating (upstream's hot0: past 8 a territory used to stay unjudged);
--   the guard wanted - a third of its citizens, 3 at least, 8 when enemy soldiers were seen there in the last
--        TV_FOE_MEMORY, TV_GUARD at most, land it holds only (a sea's guard is the fleet's);
--   holds (TV_HOLDS) - its FINISHED buildings that shoot or train there, never a wall's piece, never a Fortress
--        (the player 2026-11-15: "a settlement or a stray house should not count"); one holds the territory;
--   front and backland - a land territory it holds, or a sea it claims, is a FRONT when it borders ground or water
--        nobody of its side holds (the open border and the shore, in tiles), BACKLAND when everything round it is;
--   the seas' claim - a sea is claimed by the land along it: a side shared with a land territory it holds (a
--        corner's touch claims nothing, TV_SHORE tiles of shore); an ally's land makes it friendly; a BORDER SEA has
--        the enemy's buildings on land along it too - claimed by both, a front for both, never friendly; an OPEN SEA
--        has no land along it, claimed by nobody. The warships in it are the threat, not the claim.
--   the refuge - a Fortress with room on that land (its transport capacity) within TV_REACH, else the nearest drop
--        site of its in a territory that is not being lost, on the same land.
-- The facts run every 2 s; the roles, the border, the shore and the seas every 10 s (a deviation of cadence only:
-- the ground and the water do not move, and the pass over every cell of a land mass is the expensive one).
--
-- BORDER TOWERS ([ai_base] border_towers; M.border_tower): while BW_STONE of stone and BW_WOOD of wood lie in the
-- bank, one tower every BW_EVERY on the outside border of a territory it holds (3 of its citizens there, not being
-- lost) - a cell of it beside water or land nobody of its side holds - where no defence of its stands within
-- BW_SPACE tiles; the place nearest that territory's middle first, so the cover grows from the core outwards. THE
-- STONE SINK ([ai_base] stone_sink): with BW_SINK of stone the cover comes denser and faster - BW_SINK_SPACE apart,
-- BW_SINK_OPEN going up at a time, a look every BW_SINK_EVERY, the wood not asked (a tower costs stone only).
-- M.border_tower hands out the SPOT and the tower class; econ.lua's towers (or whoever carries the build verb out)
-- gives the order (see UPSTREAM for the wiring).
--
-- BORDER WALLS ([ai_base] border_walls; M.border_wall, M.border_runs): while BWL_STONE of stone lies in the bank, a
-- wall along the shortest border between a land territory it holds (3 citizens, not being lost) and land nobody of
-- its side holds and it has nothing in, with a gate in the middle (two on a line of BWL_GATE2 tiles: a third in
-- from each end); where no gate can be, one tile of the line stays open (a gap), and a border where neither can be
-- is not walled at all (never shutting something in). BWL_RUNS runs of up to 12 pieces a look, the stone above
-- BWL_KEEP only. The line is the CELL BOUNDARY (deviation): the tiles of the holder's side of every pair of
-- adjacent cells, joined and sorted along the border - the real tile line inside a cell is not known, the cells are
-- sampled. A border walled once is not taken up again (upstream parks it 10 minutes and finds no open tile then).
-- A border nothing happened on for BWL_STUCK waits 10 minutes. M.border_wall hands out the PLAN (line, gates, gaps,
-- runs); walls.lua (or whoever lays walls) carries the orders out.
--
-- Cost: the model once (96 cells a look every 10 s, so a 256-tile map is known in about 2 minutes) and then only as
-- new ground is explored; the facts every TV_EVERY (a scan of the own units and two of the memory - the price
-- upstream pays too) and the roles every 5th of them; the queries are array lookups. Nothing here calls the engine
-- but ai.map, ai.tile, ai.units, ai.get, ai.memory, ai.seen, ai.class, ai.resources, ai.players, ai.find_site and
-- ai.can_place.
local V = require("lib.view")
local S = require("ageofai.settings")

local M = {}

-- the model's grid
local TC_CELL = 8            -- tiles a cell (fronts.lua reads 8-tile cells too)
local TC_SIDE = 128          -- a 1024-tile map; a bigger one folds its far cells onto the edge
local TC_SEED = 5            -- a seed every 5 cells (~40 tiles): about 40 territories on a 256-tile map
local TC_MAX = 256           -- territories at most (upstream's maps had 40)
local TC_SAMPLE = 96         -- cells classified a look
local TC_GROW = 384          -- cells labelled a step (the watershed, chunked)
local TC_LOOK = 10000        -- a classification look every 10 s
local TC_ORPHAN = 128        -- cells looked at a step for a seed while the grown ones are exhausted

-- territories.c's constants
local TV_SHORE = 5           -- tiles of shore along a held territory that claim a sea (a cell side is TC_CELL wide)
local TV_HELD = 3            -- an ally's buildings that make its land friendly
local TV_GUARD = 12
local TV_FOE_MEMORY = 300000
local TV_EVERY = 2000
local TV_MEMORY = 60000
local TV_FORCE = 2
local TV_NEAR = 8            -- an enemy soldier counts for every territory this near to it (a cell)
local TV_LOSING = 100        -- percent: the guard under this against the foes is being lost (1.0 upstream)
local TV_HOLD = 100          -- ... and safe again from this on (1.0 upstream: no holding on)
local TV_CALM = 20000
local TV_HOT = 8             -- territories judged a pass
local TV_REACH = 30          -- a Fortress this near takes the citizens of a territory being lost (sites.c)
local TVR_NONE, TVR_FRONT, TVR_BACK = 0, 1, 2
local TV_QUIET, TV_FOES, TV_UNSAFE = 0, 1, 2
local ROLES_EVERY = 5        -- the roles and the seas every 5th look (10 s; a deviation of cadence only)

-- territories.c BORDER TOWERS / THE STONE SINK
local BW_EVERY = 20000
local BW_STONE = 1500
local BW_WOOD = 1500
local BW_SPACE = 12
local BW_SINK = 5000
local BW_SINK_SPACE = 7
local BW_SINK_OPEN = 4
local BW_SINK_EVERY = 10000
local BW_BAD = 32            -- bad places (5 minutes): a tower was queued for them or none could be put by
local BW_BAD_MS = 300000

-- territories.c BORDER WALLS
local BWL_EVERY = 20000
local BWL_STONE = 1500
local BWL_KEEP = 1000
local BWL_MIN = 3
local BWL_GATE2 = 48
local BWL_RUNS = 2
local BWL_RUN = 12           -- a run of wall pieces at most
local BWL_STUCK = 300000

-- the classes (the port's names; econ.lua's TOWER_CLASSES, oldest first)
local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_digital", "b_guard_tower_space"}
local TOWNS = {b_town_center = true, b_capitol = true}   -- the producers a territory always holds on
local DROPS = {b_town_center = true, b_capitol = true, b_settlement = true}
local FORTRESS = "b_fortress"
local SOLDIERS = {soldier = true, siege = true, hero = true}   -- tv_own_soldier / tv_foe_unit

local SIDES = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}}   -- the 4 ways: a side, not a corner (a corner claims nothing)
local RING = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}}

-- the constants the consumers (and the report) name, kept as the module's own
M.TVR_NONE, M.TVR_FRONT, M.TVR_BACK = TVR_NONE, TVR_FRONT, TVR_BACK
M.TV_QUIET, M.TV_FOES, M.TV_UNSAFE = TV_QUIET, TV_FOES, TV_UNSAFE
M.TV_GUARD, M.TV_FOE_MEMORY, M.TV_MEMORY, M.TV_FORCE, M.TV_CALM =
    TV_GUARD, TV_FOE_MEMORY, TV_MEMORY, TV_FORCE, TV_CALM
M.TV_SHORE, M.TV_HELD, M.TV_NEAR, M.TV_REACH = TV_SHORE, TV_HELD, TV_NEAR, TV_REACH
M.BW_STONE, M.BW_WOOD, M.BW_SPACE, M.BW_SINK, M.BW_SINK_SPACE = BW_STONE, BW_WOOD, BW_SPACE, BW_SINK, BW_SINK_SPACE
M.BWL_STONE, M.BWL_KEEP, M.BWL_GATE2, M.BWL_RUN = BWL_STONE, BWL_KEEP, BWL_GATE2, BWL_RUN
M.TC_CELL, M.TC_SEED = TC_CELL, TC_SEED

-- the model of the step under way (the AI's own, as econ.lua's cur_memory: one Lua state per seat)
local cur = nil

local function say(fmt, ...)
    print("territories " .. string.format(fmt, ...))
end

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function is_wall(t)
    t = t or ""
    return t:sub(1, 7) == "b_wall_" or t:sub(1, 7) == "b_gate_"
end

local function is_tower(t)
    t = t or ""
    return t:sub(1, 8) == "b_tower_" or t:sub(1, 14) == "b_guard_tower_"
end

-- the newest tower the player can build now (econ.lua's M.tower_class, kept local: this module is required by the
-- consumers, not the other way round)
local function tower_class(view)
    for k = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[k]) ~= nil then
            return TOWER_CLASSES[k]
        end
    end
    return nil
end

-- the shelter's level: 0 off, 1 the citizens of a lost territory into the refuge, 2 also the outposts' Fortress
local function shelter_level()
    local v = S.territory_shelter
    if v == false then
        return 0
    end
    if type(v) == "number" then
        return v
    end
    return 1   -- upstream ships the key on (config/ageofai.ini)
end
M.shelter_level = shelter_level

-- ---------------------------------------------------------------------------------------------------------------
-- The grid and the cell model

local function cell_of(t, x, y)
    local side = t.side
    if side == nil or side < 1 or x == nil or y == nil then
        return 0
    end
    local cx, cy = x // TC_CELL, y // TC_CELL
    if cx < 0 or cy < 0 then
        return 0
    end
    -- a map bigger than the grid folds its far cells onto the edge (fronts.lua's cell_of)
    if cx >= side then
        cx = side - 1
    end
    if cy >= side then
        cy = side - 1
    end
    return cy * side + cx + 1
end

local function region_at(t, x, y)
    if t == nil then
        return nil
    end
    local c = cell_of(t, x, y)
    if c == 0 then
        return nil
    end
    local r = t.lab[c]
    return r ~= 0 and r or nil
end
M.region_at = region_at

local function grid(t)
    if t.side ~= nil and t.side > 0 then
        return
    end
    local map = ai.map()
    local size = math.max(map.width or 0, map.height or 0)
    t.size = size
    t.side = math.min((size + TC_CELL - 1) // TC_CELL, TC_SIDE)
    t.ncell = t.side * t.side
    t.cls, t.lab, t.area, t.dist = {}, {}, {}, {}
    for i = 1, t.ncell do
        t.cls[i], t.lab[i], t.area[i], t.dist[i] = 0, 0, 0, 0
    end
    t.cursor, t.unknown = 1, t.ncell
    t.seed_cursor = 1
    t.q, t.qh = {}, 1
    t.uf = {}
    t.next_area = 1
    t.n = 0
    -- per territory (grown as they are made)
    t.rland, t.rcells, t.rsx, t.rsy = {}, {}, {}, {}
    t.rcont, t.rocean = {}, {}
    t.cells_of = {}
    t.foe, t.sold, t.def, t.hold, t.cit, t.bld = {}, {}, {}, {}, {}, {}
    t.ships, t.eships, t.abld, t.ebld = {}, {}, {}, {}
    t.want, t.state, t.role, t.friendly = {}, {}, {}, {}
    t.open_sea, t.border = {}, {}
    t.open, t.shore, t.coast, t.ratio = {}, {}, {}, {}
    t.fish, t.fish_t = {}, {}
    t.unsafe_t, t.foe_t, t.eship_t = {}, {}, {}
    t.nbr = {}
    t.seas, t.held, t.land_front = {}, 0, 0
    t.hot0, t.turned, t.look_t, t.log_t, t.look_n = 1, 0, nil, nil, 0
    t.cells_t = nil
end

-- the area of a cell (union-find over the known cells of one class)
local function find_root(t, a)
    local uf = t.uf
    local seen = 0
    while uf[a] ~= nil and uf[a] ~= a and seen < 64 do
        a = uf[a]
        seen = seen + 1
    end
    return a
end

local function account(t, k, i)
    local side = t.side
    local cx, cy = (i - 1) % side, (i - 1) // side
    t.rcells[k] = t.rcells[k] + 1
    t.rsx[k] = t.rsx[k] + cx * TC_CELL + TC_CELL // 2
    t.rsy[k] = t.rsy[k] + cy * TC_CELL + TC_CELL // 2
    local list = t.cells_of[k]
    if list == nil then
        list = {}
        t.cells_of[k] = list
    end
    list[#list + 1] = i
end

local function new_region(t, i)
    if t.n >= TC_MAX then
        return nil
    end
    t.n = t.n + 1
    local k = t.n
    t.rland[k] = t.cls[i] == 1
    t.rcells[k], t.rsx[k], t.rsy[k] = 0, 0, 0
    t.cells_of[k] = {}
    t.foe[k], t.sold[k], t.def[k], t.hold[k], t.cit[k], t.bld[k] = 0, 0, 0, 0, 0, 0
    t.ships[k], t.eships[k], t.abld[k], t.ebld[k] = 0, 0, 0, 0
    t.want[k], t.state[k], t.role[k], t.friendly[k] = 0, TV_QUIET, TVR_NONE, 0
    t.open_sea[k], t.border[k], t.open[k], t.shore[k], t.coast[k], t.ratio[k] = 0, 0, 0, 0, 0, 0
    t.fish[k], t.fish_t[k] = 0, nil
    t.unsafe_t[k], t.foe_t[k], t.eship_t[k] = 0, 0, 0
    t.nbr[k] = {}
    -- the engine's own id: the land mass of a land territory, the sea of a water one (ai.tile, explored)
    local side = t.side
    local tile = ai.tile((i - 1) % side * TC_CELL + TC_CELL // 2, (i - 1) // side * TC_CELL + TC_CELL // 2)
    if t.rland[k] then
        t.rcont[k] = tile ~= nil and (tile.continent or 0) or 0
        t.rocean[k] = 0
    else
        t.rcont[k] = 0
        t.rocean[k] = tile ~= nil and (tile.sea or 0) or 0
    end
    t.lab[i] = k
    t.dist[i] = 0
    account(t, k, i)
    t.q[#t.q + 1] = i
    return k
end

local function is_seed(i, side)
    local cx, cy = (i - 1) % side, (i - 1) // side
    return cx % TC_SEED == TC_SEED // 2 and cy % TC_SEED == TC_SEED // 2
end

-- a classified cell joins the territory of the nearest labelled neighbour (the incremental half of the watershed)
local function adopt(t, i)
    local side = t.side
    local x, y = (i - 1) % side, (i - 1) // side
    local best, bd = nil, nil
    for dy = -1, 1 do
        for dx = -1, 1 do
            if dx ~= 0 or dy ~= 0 then
                local nx, ny = x + dx, y + dy
                if nx >= 0 and ny >= 0 and nx < side and ny < side then
                    local j = ny * side + nx + 1
                    if t.cls[j] == t.cls[i] and t.lab[j] ~= 0 then
                        local d = t.dist[j]
                        if bd == nil or d < bd or (d == bd and t.lab[j] < best) then
                            bd, best = d, t.lab[j]
                        end
                    end
                end
            end
        end
    end
    if best ~= nil then
        t.lab[i] = best
        t.dist[i] = bd + 1
        account(t, best, i)
        t.q[#t.q + 1] = i
    end
end

local function set_cell(t, i, cls)
    t.cls[i] = cls
    t.unknown = t.unknown - 1
    local side = t.side
    local x, y = (i - 1) % side, (i - 1) // side
    local a = 0
    for dy = -1, 1 do
        for dx = -1, 1 do
            if dx ~= 0 or dy ~= 0 then
                local nx, ny = x + dx, y + dy
                if nx >= 0 and ny >= 0 and nx < side and ny < side then
                    local j = ny * side + nx + 1
                    if cls < 3 and t.cls[j] == cls then
                        local aj = find_root(t, t.area[j])
                        if a == 0 then
                            a = aj
                        elseif aj ~= a then
                            if aj < a then
                                t.uf[a] = aj
                                a = aj
                            else
                                t.uf[aj] = a
                            end
                        end
                    end
                end
            end
        end
    end
    if a == 0 and cls < 3 then
        a = t.next_area
        t.next_area = a + 1
        t.uf[a] = a
    end
    t.area[i] = a
    if cls < 3 then
        if is_seed(i, side) then
            new_region(t, i)
        else
            adopt(t, i)
        end
    end
end

-- the classifications of a look: land (1), water (2), other (3: a forest, a cliff, the map's edge), unknown (0,
-- looked at again later). 9 of the cell's 64 tiles, explored ones only (fog: the terrain is not known before seen)
local function classify(t, budget)
    local side, n = t.side, t.ncell
    local looked, i, steps = 0, t.cursor, 0
    while t.unknown > 0 and looked < budget and steps < n do
        steps = steps + 1
        if t.cls[i] == 0 then
            looked = looked + 1
            local x0, y0 = (i - 1) % side * TC_CELL, (i - 1) // side * TC_CELL
            local land, water, explored = 0, 0, 0
            for dy = 1, 7, 3 do
                for dx = 1, 7, 3 do
                    local tile = ai.tile(x0 + dx, y0 + dy)
                    if tile == nil then
                        explored = explored + 1   -- off the map (a folded cell)
                    elseif tile.explored then
                        explored = explored + 1
                        if (tile.continent or 0) ~= 0 then
                            land = land + 1
                        elseif (tile.sea or 0) ~= 0 then
                            water = water + 1
                        end
                    end
                end
            end
            local cls = 0
            if land >= 2 then
                cls = 1
            elseif water >= 2 then
                cls = 2
            elseif explored >= 9 then
                cls = 3
            end
            if cls ~= 0 then
                set_cell(t, i, cls)
            end
        end
        i = i % n + 1
        if i == t.cursor then
            break
        end
    end
    t.cursor = i
end

-- the watershed: every labelled cell expands into the unlabelled ones of its class, in the order it was labelled
-- (a multi-source BFS: the first frontier to reach a cell takes it, so the borders fall where the distances meet)
local function grow(t, budget)
    local side = t.side
    local done = 0
    while t.qh <= #t.q and done < budget do
        local c = t.q[t.qh]
        t.qh = t.qh + 1
        done = done + 1
        local k, cls = t.lab[c], t.cls[c]
        if k ~= 0 and cls ~= 0 then
            local x, y = (c - 1) % side, (c - 1) // side
            local d = t.dist[c] + 1
            for dy = -1, 1 do
                for dx = -1, 1 do
                    if dx ~= 0 or dy ~= 0 then
                        local nx, ny = x + dx, y + dy
                        if nx >= 0 and ny >= 0 and nx < side and ny < side then
                            local j = ny * side + nx + 1
                            if t.lab[j] == 0 and t.cls[j] == cls then
                                t.lab[j] = k
                                t.dist[j] = d
                                account(t, k, j)
                                t.q[#t.q + 1] = j
                            end
                        end
                    end
                end
            end
        end
    end
    if t.qh > #t.q then
        t.q, t.qh = {}, 1
    end
end

-- a land mass the lattice does not seed (a small island between the seeds): its first classified cell seeds it once
-- the grown territories have stopped, so that no cell stays without a territory
local function seed_orphans(t, budget)
    if t.qh <= #t.q or t.n >= TC_MAX or t.unknown == t.ncell then
        return
    end
    local n = t.ncell
    local i, looked = t.seed_cursor, 0
    while looked < budget do
        if t.cls[i] ~= 0 and t.cls[i] < 3 and t.lab[i] == 0 then
            new_region(t, i)
            break
        end
        looked = looked + 1
        i = i % n + 1
        if i == t.seed_cursor then
            break
        end
    end
    t.seed_cursor = i
end

-- the territories a soldier at (x, y) counts for: its own and those within TV_NEAR tiles (the eight ways, the cell
-- and its neighbours), as territories.c's tv_reach
local function reach_of(t, x, y)
    local c = cell_of(t, x, y)
    local out = {}
    if c == 0 then
        return out
    end
    local side = t.side
    local cx, cy = (c - 1) % side, (c - 1) // side
    for _, dd in ipairs(RING) do
        local nx, ny = cx + dd[1], cy + dd[2]
        if nx >= 0 and ny >= 0 and nx < side and ny < side then
            local r = t.lab[ny * side + nx + 1]
            if r ~= 0 then
                local dup = false
                for _, o in ipairs(out) do
                    if o == r then
                        dup = true
                        break
                    end
                end
                if not dup then
                    out[#out + 1] = r
                end
            end
        end
    end
    return out
end

-- ---------------------------------------------------------------------------------------------------------------
-- The facts of a territory (tv_tick's counting)

local function reset(t)
    for r = 1, t.n do
        t.foe[r], t.sold[r], t.def[r], t.hold[r], t.cit[r], t.bld[r] = 0, 0, 0, 0, 0, 0
        t.ships[r], t.abld[r], t.ebld[r] = 0, 0, 0
        t.want[r], t.ratio[r] = 0, 0
    end
end

-- the buildings a territory holds on: one finished building of its that shoots or trains (a Tower, a Town Center,
-- a Capitol, a Barracks and the other production buildings, a Dock; not a Fortress) - tv_holder
local function holder_names(view)
    local names = {b_town_center = true, b_capitol = true}
    for _, c in ipairs(view.costs or {}) do
        if c.producer ~= nil then
            names[c.producer] = true
        end
    end
    return names
end

-- the own units: the citizens, buildings (bld, def, hold), soldiers and warships by territory; the soldiers and
-- the defences also as strength for the fight at a territory
local function own_units(view, t, own)
    local ids = ai.units{}
    if #ids == 0 then
        return
    end
    local cols = ai.get(ids, {"x", "y", "type", "category", "built", "hp"})
    local holds = holder_names(view)
    for k = 1, #ids do
        local x, y = cols.x[k], cols.y[k]
        local cat = cols.category[k]
        if x ~= nil and y ~= nil and x >= 0 and cat ~= nil then
            local r = region_at(t, x, y)
            if cat == "citizen" then
                if r ~= nil then
                    t.cit[r] = t.cit[r] + 1
                end
            elseif cat == "building" then
                local ty = cols.type[k]
                if r ~= nil and ty ~= nil and not is_wall(ty) then
                    t.bld[r] = t.bld[r] + 1
                    local cls = ai.class(ty) or {}
                    local atk = cls.attack or 0
                    if atk > 0 then
                        t.def[r] = t.def[r] + 1
                        own[#own + 1] = {x = x, y = y, s = (cols.hp[k] or cls.hp or 0) * atk}
                    end
                    if cols.built[k] and (atk > 0 or holds[ty] or TOWNS[ty]) then
                        t.hold[r] = t.hold[r] + 1
                    end
                end
            elseif SOLDIERS[cat] then
                if r ~= nil then
                    t.sold[r] = t.sold[r] + 1
                end
                local cls = ai.class(cols.type[k]) or {}
                if (cls.attack or 0) > 0 then
                    own[#own + 1] = {x = x, y = y, s = (cols.hp[k] or cls.hp or 0) * cls.attack}
                end
            elseif cat == "ship" then
                local cls = ai.class(cols.type[k]) or {}
                if r ~= nil and (cls.attack or 0) > 0 and not cls.is_transport then
                    t.ships[r] = t.ships[r] + 1
                end
            end
        end
    end
end

-- the enemy soldiers it saw there and doesn't see now, and those it sees: the memory's records of the last
-- TV_MEMORY (upstream skips the record of the sight pass itself; here one window covers both - a deviation in the
-- counts' timing only, never in what is read)
local function foe_units(t, now, foes)
    for _, q in ipairs(ai.memory{since = math.max(now - TV_MEMORY, 0)}) do
        if not q.building and q.x ~= nil and q.y ~= nil and q.x >= 0 and q.y >= 0 then
            local cls = ai.class(q.type)
            if cls ~= nil and SOLDIERS[cls.category] and (cls.attack or 0) > 0 then
                local rs = reach_of(t, q.x, q.y)
                for _, r in ipairs(rs) do
                    t.foe[r] = t.foe[r] + 1
                end
                foes[#foes + 1] = {x = q.x, y = q.y, s = (q.hp or cls.hp or 0) * cls.attack, rs = rs}
            end
        end
    end
end

-- the enemy buildings it knows (in sight or remembered: what a human sees painted on the fog) and - every
-- ROLES_EVERY looks, the charge on the memory - the enemy warships it saw in the last TV_FOE_MEMORY (the harbour's
-- second tower and the fleet's station read them)
local function enemy_buildings_and_ships(t, now, ships)
    for _, q in ipairs(ai.memory{buildings = true}) do
        if q.x ~= nil and q.y ~= nil and q.x >= 0 and q.y >= 0 and not is_wall(q.type) then
            local r = region_at(t, q.x, q.y)
            if r ~= nil then
                t.ebld[r] = t.ebld[r] + 1
            end
        end
    end
    if not ships then
        return
    end
    for r = 1, t.n do
        t.eships[r] = 0   -- eship_t stays: the last time a warship was seen there (the harbour's, the station's window)
    end
    for _, q in ipairs(ai.memory{since = math.max(now - TV_FOE_MEMORY, 0)}) do
        if not q.building and q.x ~= nil and q.x >= 0 and q.y ~= nil then
            local cls = ai.class(q.type) or {}
            if cls.category == "ship" and (cls.attack or 0) > 0 and not cls.is_transport then
                local r = region_at(t, q.x, q.y)
                if r ~= nil then
                    t.eships[r] = t.eships[r] + 1
                    t.eship_t[r] = now > 0 and now or 1
                end
            end
        end
    end
end

-- the allies' buildings it sees and the fish in the seas: every ROLES_EVERY looks (the charge on ai.seen)
local function allies_and_fish(t, view, now)
    for r = 1, t.n do
        t.abld[r] = 0
        t.nbr[r] = t.nbr[r] or {}
        if not t.rland[r] then
            t.fish[r] = 0
        end
    end
    local ally = {}
    for _, p in ipairs(ai.players()) do
        if p.ally and not p.me then
            ally[p.id] = true
        end
    end
    local ids = ai.seen{near = {view.town_x or 0, view.town_y or 0, 2000}}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "type", "player", "category", "built"})
        for k = 1, #ids do
            if ally[cols.player[k]] and cols.category[k] == "building" and cols.built[k]
                    and cols.x[k] ~= nil and cols.x[k] >= 0 then
                local r = region_at(t, cols.x[k], cols.y[k])
                if r ~= nil and not is_wall(cols.type[k]) then
                    t.abld[r] = t.abld[r] + 1
                end
            end
        end
    end
    local fish = ai.resources{kind = "food", type = "res_fish", limit = 512}
    if #fish > 0 then
        local cols = ai.get(fish, {"x", "y", "amount"})
        for k = 1, #fish do
            local r = region_at(t, cols.x[k] or -1, cols.y[k] or -1)
            if r ~= nil and not t.rland[r] then
                t.fish[r] = t.fish[r] + math.max(cols.amount[k] or 0, 0)
            end
        end
    end
    for r = 1, t.n do
        t.fish_t[r] = now
    end
end

-- the guard against the foes, TV_HOT territories a pass (upstream's hot list and its rotation); own and foes are
-- the strength lists of the look (hit points x attack, the port's measure without the damage tables)
local function judge(t, now, own, foes)
    local hot, nh, nq = {}, 0, 0
    for r = 1, t.n do
        if t.foe[r] >= TV_FORCE and (t.cit[r] > 0 or t.def[r] > 0 or t.sold[r] > 0) then
            nq = nq + 1
        end
    end
    local r0 = nq > TV_HOT and t.hot0 > 0 and t.hot0 <= t.n and t.hot0 or 1
    local k = 0
    while k < t.n and nh < TV_HOT do
        local r = (r0 - 1 + k) % t.n + 1
        if t.foe[r] >= TV_FORCE and (t.cit[r] > 0 or t.def[r] > 0 or t.sold[r] > 0) then
            nh = nh + 1
            hot[nh] = r
        end
        k = k + 1
    end
    t.hot0 = (nq > TV_HOT and nh > 0) and (hot[nh] % t.n + 1) or 1
    if nq > TV_HOT then
        t.crowded = (t.crowded or 0) + 1
        if nq > (t.crowd_max or 0) then
            t.crowd_max = nq
        end
    end
    local slot, hx, hy = {}, {}, {}
    for h = 1, nh do
        local r = hot[h]
        slot[r] = h
        local n = math.max(t.rcells[r], 1)
        hx[h], hy[h] = t.rsx[r] // n, t.rsy[r] // n
    end
    local a, b = {}, {}
    for h = 1, nh do
        a[h], b[h] = 0, 0
    end
    local function add(list, sums)
        for _, u in ipairs(list) do
            local rs = u.rs or reach_of(t, u.x, u.y)
            for _, r in ipairs(rs) do
                local h = slot[r]
                if h ~= nil and math.abs(u.x - hx[h]) <= 120 and math.abs(u.y - hy[h]) <= 120 then
                    sums[h] = sums[h] + u.s
                end
            end
        end
    end
    add(own, a)
    add(foes, b)
    for h = 1, nh do
        local r = hot[h]
        local ratio = b[h] <= 0 and 1000000 or (a[h] * 100 // b[h])
        t.ratio[r] = ratio
        if ratio < (t.state[r] == TV_UNSAFE and TV_HOLD or TV_LOSING) then
            t.unsafe_t[r] = now > 0 and now or 1
        end
    end
    for r = 1, t.n do
        local was = t.state[r]
        if t.unsafe_t[r] > 0 and now >= t.unsafe_t[r] and now - t.unsafe_t[r] < TV_CALM then
            t.state[r] = TV_UNSAFE
        elseif t.foe[r] > 0 then
            t.state[r] = TV_FOES
        else
            t.state[r] = TV_QUIET
        end
        if t.state[r] ~= TV_UNSAFE then
            t.unsafe_t[r] = 0
        end
        if (was == TV_UNSAFE) ~= (t.state[r] == TV_UNSAFE) then
            if t.state[r] == TV_UNSAFE then
                t.turned = (t.turned or 0) + 1
            end
            if S.territories_log then
                local n = math.max(t.rcells[r], 1)
                say("P%d at %.1f min: territory %d at %d,%d %s: %d enemy soldiers against %d soldiers and %d defences (ratio %.2f), %d citizens there",
                    t.player or 0, now / 60000.0, r, t.rsx[r] // n, t.rsy[r] // n,
                    t.state[r] == TV_UNSAFE and "is being lost" or "safe again", t.foe[r], t.sold[r], t.def[r],
                    (t.ratio[r] or 0) / 100.0, t.cit[r])
            end
        end
    end
end

-- the guard a held one would want (read by army.lua, towns.lua, expand.c's front guards)
local function wants(t, now)
    for r = 1, t.n do
        if t.foe[r] > 0 then
            t.foe_t[r] = now > 0 and now or 1
        end
        local w = t.cit[r] // 3
        if w < 3 then
            w = 3
        end
        if t.foe_t[r] > 0 and now >= t.foe_t[r] and now - t.foe_t[r] < TV_FOE_MEMORY and w < 8 then
            w = 8
        end
        t.want[r] = (t.rland[r] and t.hold[r] >= 1) and math.min(w, TV_GUARD) or 0
    end
end

local function nbr_add(t, r, o)
    local list = t.nbr[r]
    local k = 1
    while k <= #list do
        if list[k] == o then
            list[k + 1] = list[k + 1] + TC_CELL
            return
        end
        k = k + 2
    end
    list[#list + 1] = o
    list[#list + 1] = TC_CELL
end

-- FRONT AND BACKLAND and THE SEAS' CLAIM: what it or an ally holds, the open border and the shore in tiles, the
-- seas' land along them and their coast; the neighbours the front station draws the border's weight from
local function roles(t)
    local shore_claims = TC_CELL >= TV_SHORE   -- a cell side is TV_SHORE tiles of shore or more: it claims
    local wnb = {}                             -- a land territory's seas and their lengths (the second half below)
    for r = 1, t.n do
        t.open[r], t.shore[r], t.coast[r], t.border[r], t.open_sea[r] = 0, 0, 0, 0, 0
        t.nbr[r] = {}
        if t.rland[r] then
            t.role[r] = t.hold[r] >= 1 and TVR_FRONT or TVR_NONE
            t.friendly[r] = (t.role[r] ~= TVR_NONE or t.abld[r] >= TV_HELD) and 1 or 0
            t.open_sea[r] = 0
        else
            t.role[r], t.friendly[r], t.open_sea[r] = TVR_NONE, 0, 1
        end
    end
    -- the land's cells: its neighbours and its open border, and what each sea's land along it says about the sea
    local sany, sun, sfoe, sown, scoast = {}, {}, {}, {}, {}
    for r = 1, t.n do
        if t.rland[r] then
            wnb[r] = {}
            for _, i in ipairs(t.cells_of[r]) do
                local x, y = (i - 1) % t.side, (i - 1) // t.side
                for _, d in ipairs(SIDES) do
                    local nx, ny = x + d[1], y + d[2]
                    if nx >= 0 and ny >= 0 and nx < t.side and ny < t.side then
                        local o = t.lab[ny * t.side + nx + 1]
                        if o ~= 0 and o ~= r then
                            if t.rland[o] then
                                nbr_add(t, r, o)
                                if t.friendly[o] == 0 then
                                    t.open[r] = t.open[r] + TC_CELL
                                end
                            else
                                -- one of the sea o's land borders: claimed by a holder, friendly by a side, a border
                                -- by an enemy's building, open (its shore) by land nobody of that side holds
                                t.open_sea[o] = 0
                                if t.friendly[r] == 1 then
                                    sany[o] = true
                                else
                                    sun[o] = true
                                    t.shore[o] = t.shore[o] + TC_CELL
                                end
                                if t.ebld[r] > 0 then
                                    sfoe[o] = true
                                end
                                if t.role[r] ~= TVR_NONE then
                                    sown[o] = true
                                    scoast[o] = (scoast[o] or 0) + TC_CELL
                                end
                                local list = wnb[r]
                                local k = 1
                                local found = false
                                while k <= #list and not found do
                                    if list[k] == o then
                                        list[k + 1] = list[k + 1] + TC_CELL
                                        found = true
                                    end
                                    k = k + 2
                                end
                                if not found then
                                    list[#list + 1] = o
                                    list[#list + 1] = TC_CELL
                                end
                            end
                        end
                    end
                end
            end
        end
    end
    -- the seas: claimed by the land along them (TV_SHORE tiles of shore at least), friendly when every land along
    -- them is its side's and no enemy's building is, a border sea (the enemy's land too) a front for both
    for r = 1, t.n do
        if not t.rland[r] then
            t.coast[r] = scoast[r] or 0
            t.border[r] = sfoe[r] and 1 or 0
            if sown[r] == true and shore_claims then
                t.role[r] = TVR_FRONT
                -- DEVIATION: a sea is one territory per water body, so a land nobody of its side holds anywhere along
                -- it keeps the whole sea unfriendly (upstream's smaller water regions were friendly piece by piece)
                t.friendly[r] = (sany[r] == true and sun[r] ~= true and sfoe[r] ~= true) and 1 or 0
            end
        end
    end
    -- a land territory's shore: the sea along it now that the sea's side is known (a friendly sea is no shore)
    for r = 1, t.n do
        local list = wnb[r]
        if list ~= nil then
            for k = 1, #list, 2 do
                if t.friendly[list[k]] == 0 then
                    t.shore[r] = t.shore[r] + list[k + 1]
                end
            end
        end
    end
    -- the role: front when anything it borders is not its side's
    for r = 1, t.n do
        if t.role[r] ~= TVR_NONE then
            t.role[r] = (t.open[r] > 0 or t.shore[r] > 0 or t.border[r] == 1) and TVR_FRONT or TVR_BACK
        end
    end
end

-- tv_seas: its waters by the engine's sea, open (a front) or closed, the shore along its land, the fish, the need
local function seas(t)
    local list, byocean = {}, {}
    t.held, t.land_front = 0, 0
    for r = 1, t.n do
        if t.rland[r] and t.role[r] ~= TVR_NONE then
            t.held = t.held + 1
            t.land_front = t.land_front + t.open[r]
        end
    end
    for r = 1, t.n do
        if not t.rland[r] and t.role[r] ~= TVR_NONE then
            local ocean = t.rocean[r]
            local e = ocean ~= 0 and byocean[ocean] or nil
            if e == nil and #list >= 8 then
                e = nil   -- upstream's TVS_N: eight seas
            else
                if e == nil then
                    e = {ocean = ocean, waters = 0, closed = 0, area = 0, shore = 0, fish = 0, need = 0,
                         regions = {}, eship_t = 0}
                    if ocean ~= 0 then
                        byocean[ocean] = e
                    end
                    list[#list + 1] = e
                end
                e.waters = e.waters + 1
                e.shore = e.shore + (t.coast[r] or 0)
                e.fish = e.fish + (t.fish[r] or 0)
                e.regions[#e.regions + 1] = r
                if t.eship_t[r] > (e.eship_t or 0) then
                    e.eship_t = t.eship_t[r]
                end
                if t.role[r] == TVR_FRONT then
                    e.area = e.area + t.rcells[r] * TC_CELL * TC_CELL
                else
                    e.closed = e.closed + 1
                end
            end
        end
    end
    local tiles = S.sea_fleet
    if tiles == nil then
        tiles = 300    -- upstream's shipped sea_fleet
    end
    local island = t.held > 0 and t.land_front == 0   -- no land it holds borders land of others: the sea is its front
    for _, e in ipairs(list) do
        if e.area <= 0 or tiles <= 0 then
            e.need = 0
        elseif island then
            e.need = 10000   -- all of the navy's share (navy.c navy_strength cuts it)
        else
            e.need = math.max(2, (e.area + tiles // 2) // tiles)
        end
    end
    t.seas = list
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look (tv_tick)

local function log_look(t, view, now)
    if not S.territories_log then
        return
    end
    if t.log_t ~= nil and now >= t.log_t and now - t.log_t < 300000 then
        return
    end
    t.log_t = now > 0 and now or 1
    local unsafe, withfoe, held, short, fronts = 0, 0, 0, 0, 0
    for r = 1, t.n do
        if t.state[r] == TV_UNSAFE then
            unsafe = unsafe + 1
        end
        if t.state[r] == TV_FOES then
            withfoe = withfoe + 1
        end
        if t.want[r] > 0 then
            held = held + 1
            short = short + math.max(t.want[r] - t.sold[r], 0)
        end
        if t.role[r] == TVR_FRONT then
            fronts = fronts + 1
        end
    end
    local claimed, border_seas, open_seas = 0, 0, 0
    for r = 1, t.n do
        if not t.rland[r] then
            open_seas = open_seas + t.open_sea[r]
            if t.role[r] ~= TVR_NONE then
                claimed = claimed + 1
                border_seas = border_seas + t.border[r]
            end
        end
    end
    say("P%d at %.1f min: %d territories (%d land held, %d fronts), %d with enemy soldiers, %d being lost; so far %d turned unsafe, %d passes with more than %d to judge (%d at most) | seas claimed %d (%d border seas), %d open seas | %d held, %d soldiers short of the guards wanted | side=%d unknown=%d",
        view.player or 0, now / 60000.0, t.n, t.held, fronts, withfoe, unsafe, t.turned or 0, t.crowded or 0, TV_HOT,
        t.crowd_max or 0, claimed, border_seas, open_seas, held, short, t.side or 0, t.unknown or 0)
end

local function look(view, memory, t, now)
    reset(t)
    local own, foes = {}, {}
    own_units(view, t, own)
    foe_units(t, now, foes)
    t.look_n = (t.look_n or 0) + 1
    local roles_look = (t.look_n - 1) % ROLES_EVERY == 0
    enemy_buildings_and_ships(t, now, roles_look)
    judge(t, now, own, foes)
    wants(t, now)
    if roles_look then
        allies_and_fish(t, view, now)
        roles(t)
        seas(t)
    end
    log_look(t, view, now)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The queries (territories.c's tv_*)

-- the territory of a tile (nil: none, or nothing known there yet)
function M.terr_at(x, y)
    return region_at(cur, x, y)
end

-- how many territories the model has (0 before the first cells are known)
function M.count()
    return cur ~= nil and cur.n or 0
end

-- a territory's record (nil past the model)
function M.info(i)
    local t = cur
    if t == nil or i == nil or i < 1 or i > t.n then
        return nil
    end
    local n = math.max(t.rcells[i], 1)
    return {
        i = i, land = t.rland[i], cells = t.rcells[i], x = t.rsx[i] // n, y = t.rsy[i] // n,
        cont = t.rcont[i], ocean = t.rocean[i],
        state = t.state[i], role = t.role[i], friendly = t.friendly[i] == 1,
        foe = t.foe[i], sold = t.sold[i], def = t.def[i], cit = t.cit[i], bld = t.bld[i], hold = t.hold[i],
        want = t.want[i], ratio = t.ratio[i], ships = t.ships[i], eships = t.eships[i], abld = t.abld[i],
        ebld = t.ebld[i], open = t.open[i], shore = t.shore[i], coast = t.coast[i], fish = t.fish[i] or 0,
        border = t.border[i] == 1, open_sea = t.open_sea[i] == 1,
        unsafe_t = t.unsafe_t[i], foe_t = t.foe_t[i], eship_t = t.eship_t[i],
    }
end

-- armies.c HELD GROUND: AI idx holds the land territory of that tile
function M.holds(x, y)
    local t = cur
    local r = region_at(t, x, y)
    return r ~= nil and t.rland[r] == true and t.role[r] ~= TVR_NONE
end

-- is the territory of tile (x, y) being lost (tv_unsafe: false while territory_shelter is off, as upstream)
function M.unsafe(x, y)
    local t = cur
    if t == nil or shelter_level() < 1 then
        return false
    end
    local r = region_at(t, x, y)
    return r ~= nil and t.state[r] == TV_UNSAFE
end

-- a territory's being-lost state without the setting's gate (0 quiet, 1 foes, 2 unsafe)
function M.state_of(x, y)
    local t = cur
    local r = region_at(t, x, y)
    return (r ~= nil and t.state[r]) or TV_QUIET
end

-- the enemy soldiers AI idx counts in or by a territory (-1: not known)
function M.foes(i)
    local t = cur
    if t == nil or i == nil or i < 1 or i > t.n then
        return -1
    end
    return t.foe[i]
end

-- a territory's guard: its land soldiers there, and its defences standing in it
function M.guard(i)
    local t = cur
    if t == nil or i == nil or i < 1 or i > t.n then
        return 0, 0
    end
    return t.sold[i], t.def[i]
end

-- the guard a held land territory would want (0: not held, or a sea's - the fleet's)
function M.guard_want(i)
    local t = cur
    if t == nil or i == nil or i < 1 or i > t.n then
        return 0
    end
    return t.want[i]
end

-- tv_front_guard: the land territory of tile (x, y) is a front - its soldiers there, the guard wanted, the index
function M.front_guard(x, y)
    local t = cur
    local r = region_at(t, x, y)
    if r == nil or not t.rland[r] or t.role[r] ~= TVR_FRONT then
        return nil
    end
    return t.sold[r], t.want[r], r
end

-- tv_reach: the territories a soldier at (x, y) counts for (its own and those within TV_NEAR tiles)
function M.reach(x, y)
    local t = cur
    if t == nil then
        return {}
    end
    return reach_of(t, x, y)
end

-- the neighbours of a territory: {{o, len (in tiles), land}, ...} - watch.lua's ground, the fronts' weights
function M.neighbours(i)
    local t = cur
    local out = {}
    if t == nil or i == nil or i < 1 or i > t.n then
        return out
    end
    local list = t.nbr[i]
    for k = 1, #list, 2 do
        out[#out + 1] = {o = list[k], len = list[k + 1], land = t.rland[list[k]]}
    end
    return out
end

-- the engine's land id of a territory's land (a sea's engine sea id, 0 for a sea it does not know)
function M.area_of(i)
    local t = cur
    if t == nil or i == nil or i < 1 or i > t.n then
        return 0
    end
    return t.rland[i] and t.rcont[i] or t.rocean[i]
end

-- the sea territories it claims (tv_seas): {ocean, waters, closed, area, shore, fish, need, regions, eship_t}
function M.seas()
    local t = cur
    return t ~= nil and t.seas or {}
end

local function sea_record(t, arg)
    if t == nil or arg == nil or t.seas == nil then
        return nil
    end
    for _, e in ipairs(t.seas) do
        if e.ocean == arg then
            return e
        end
    end
    if type(arg) == "number" then   -- a sea territory's own index (upstream's tv_sea_threat's argument)
        for _, e in ipairs(t.seas) do
            for _, r in ipairs(e.regions) do
                if r == arg then
                    return e
                end
            end
        end
    end
    return nil
end

-- the engine's sea id of the water at a tile (nil: not a sea it knows)
function M.sea_of(x, y)
    local t = cur
    local r = region_at(t, x, y)
    if r == nil or t.rland[r] then
        return nil
    end
    return t.rocean[r]
end

-- the sea record of an engine sea id (or of a sea territory's index)
function M.sea(arg)
    return sea_record(cur, arg)
end

-- the warships its waters in that sea want (0: none of its waters there, or all of them closed)
function M.sea_need(arg)
    local e = sea_record(cur, arg)
    return e ~= nil and e.need or 0
end

-- the food in the fish it has seen in its waters in that sea
function M.sea_fish(arg)
    local e = sea_record(cur, arg)
    return e ~= nil and e.fish or 0
end

-- the sea whose waters want the most warships, and how many (0: none)
function M.sea_best()
    local t = cur
    local best, ocean = 0, nil
    for _, e in ipairs(t ~= nil and t.seas or {}) do
        if e.need > best then
            best, ocean = e.need, e.ocean
        end
    end
    return best, ocean
end

-- enemy warships seen on that sea in the last 10 minutes: its harbour gets its second tower (fleet.c)
function M.sea_threat(arg)
    local t = cur
    local e = sea_record(t, arg)
    if e == nil or t == nil then
        return false
    end
    local now = t.now or 0
    return (e.eship_t or 0) > 0 and now >= e.eship_t and now - e.eship_t < 2 * TV_FOE_MEMORY
end

-- tv_front_station: where the force stands - the land front on its base's land with the heaviest open border
-- (each border's tiles x (1 + the enemy soldiers it knows beyond it), the front's sum x (1 + its buildings there /
-- 8)), a third of the way from that territory's middle towards the neighbour the heaviest border is with
function M.front_station(view)
    local t = cur
    if t == nil or t.n == 0 then
        return nil
    end
    local bx = view ~= nil and view.town_x or t.base_x
    local by = view ~= nil and view.town_y or t.base_y
    if bx == nil or bx < 0 or by == nil or by < 0 then
        return nil
    end
    local base = region_at(t, bx, by)
    local land = base ~= nil and t.rcont[base] or 0
    if land == 0 then
        local tile = ai.tile(bx, by)
        land = tile ~= nil and (tile.continent or 0) or 0
    end
    local best, bto, bw = nil, nil, nil
    for r = 1, t.n do
        if t.rland[r] and t.role[r] == TVR_FRONT and t.open[r] > 0 and (land == 0 or t.rcont[r] == land) then
            local sum, most, to = 0, nil, nil
            local list = t.nbr[r]
            for k = 1, #list, 2 do
                local o, len = list[k], list[k + 1]
                if t.rland[o] and t.friendly[o] == 0 then
                    local w = len * (1 + t.foe[o])
                    sum = sum + w
                    if most == nil or w > most then
                        most, to = w, o
                    end
                end
            end
            sum = sum * (100 + t.bld[r] * 100 // 8) // 100
            if to ~= nil and (bw == nil or sum > bw) then
                bw, best, bto = sum, r, to
            end
        end
    end
    if best == nil then
        return nil
    end
    local cx = t.rsx[best] // math.max(t.rcells[best], 1)
    local cy = t.rsy[best] // math.max(t.rcells[best], 1)
    local nx = t.rsx[bto] // math.max(t.rcells[bto], 1)
    local ny = t.rsy[bto] // math.max(t.rcells[bto], 1)
    local outx, outy = cx, cy
    local f = 35
    while f > 0 do
        local qx = cx + (nx - cx) * f // 100
        local qy = cy + (ny - cy) * f // 100
        if region_at(t, qx, qy) == best then
            outx, outy = qx, qy
            break
        end
        f = f - 7
    end
    return outx, outy, best
end

-- the refuge of a citizen at (x, y): a Fortress with room on that land within TV_REACH, else the nearest drop site
-- of its in a territory that is not being lost and knows no enemy soldier, on the same land
-- -> x, y, the building's id, "fortress" / "drop"
function M.refuge(x, y)
    local t = cur
    if t == nil then
        return nil
    end
    local r0 = region_at(t, x, y)
    local land = r0 ~= nil and t.rcont[r0] or 0
    if land == 0 then
        local tile = ai.tile(x, y)
        land = tile ~= nil and (tile.continent or 0) or 0
    end
    local best, bd = nil, nil
    local ids = ai.units{type = FORTRESS, built = true}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "cargo"})
        local cap = (ai.class(FORTRESS) or {}).transport or 0
        for k = 1, #ids do
            local fx, fy = cols.x[k], cols.y[k]
            if fx ~= nil and fy ~= nil and fx >= 0 and cap - (cols.cargo[k] or 0) > 0 then
                local d = d2(fx, fy, x, y)
                if d <= TV_REACH * TV_REACH and (bd == nil or d < bd) then
                    local fr = region_at(t, fx, fy)
                    if land == 0 or (fr ~= nil and t.rcont[fr] == land) then
                        bd, best = d, {x = fx, y = fy, id = ids[k], kind = "fortress"}
                    end
                end
            end
        end
    end
    if best ~= nil then
        return best.x, best.y, best.id, best.kind
    end
    local drops = ai.units{category = "building", built = true}
    if #drops > 0 then
        local cols = ai.get(drops, {"x", "y", "type"})
        for k = 1, #drops do
            local dx, dy, ty = cols.x[k], cols.y[k], cols.type[k]
            if dx ~= nil and dy ~= nil and dx >= 0 and DROPS[ty] then
                local dr = region_at(t, dx, dy)
                if dr ~= nil and t.state[dr] ~= TV_UNSAFE and t.foe[dr] == 0
                        and (land == 0 or t.rcont[dr] == land) then
                    local d = d2(dx, dy, x, y)
                    if bd == nil or d < bd then
                        bd, best = d, {x = dx, y = dy, id = drops[k], kind = "drop"}
                    end
                end
            end
        end
    end
    if best == nil then
        return nil
    end
    return best.x, best.y, best.id, best.kind
end

-- ---------------------------------------------------------------------------------------------------------------
-- BORDER TOWERS ([ai_base] border_towers; territories.c tv_border_towers)

local function armed_towers(t)
    local covers, open = {}, 0
    local ids = ai.units{category = "building"}
    if #ids == 0 then
        return covers, open
    end
    local cols = ai.get(ids, {"x", "y", "type", "built"})
    for k = 1, #ids do
        local ty, x = cols.type[k], cols.x[k]
        if ty ~= nil and x ~= nil and x >= 0 and not is_wall(ty) then
            local cls = ai.class(ty) or {}
            if (cls.attack or 0) > 0 then
                if cols.built[k] then
                    covers[#covers + 1] = {x = x, y = cols.y[k]}
                elseif is_tower(ty) then
                    open = open + 1
                end
            end
        end
    end
    return covers, open
end

-- the tile of a border cell to aim a tower at: the middle of the side facing the water or the land nobody of its
-- side holds (a cell is 8 tiles wide: the cell's middle would stand too far back)
local function border_point(t, i)
    local x, y = (i - 1) % t.side, (i - 1) // t.side
    local self = t.lab[i]
    for _, d in ipairs(SIDES) do
        local nx, ny = x + d[1], y + d[2]
        if nx < 0 or ny < 0 or nx >= t.side or ny >= t.side then
            return x * TC_CELL + TC_CELL // 2, y * TC_CELL + TC_CELL // 2
        end
        local o = t.lab[ny * t.side + nx + 1]
        if o ~= 0 and o ~= self and (not t.rland[o] or t.friendly[o] == 0) then
            return x * TC_CELL + TC_CELL // 2 + d[1] * (TC_CELL // 2 - 1),
                   y * TC_CELL + TC_CELL // 2 + d[2] * (TC_CELL // 2 - 1)
        end
    end
    return nil
end

local function raiders_near(x, y, r)
    local ids = ai.seen{enemy = true, armed = true, near = {x, y, r}}
    if #ids == 0 then
        return false
    end
    local cols = ai.get(ids, {"category"})
    for k = 1, #ids do
        if SOLDIERS[cols.category[k]] then
            return true
        end
    end
    return false
end

local function bad_near(bw, x, y, r)
    for _, b in ipairs(bw.bad or {}) do
        if b.t ~= nil and d2(b.x, b.y, x, y) <= r * r then
            return true
        end
    end
    return false
end

local function bad_add(bw, x, y, now)
    bw.bad = bw.bad or {}
    bw.nbad = (bw.nbad or 0) + 1
    local k = (bw.nbad - 1) % BW_BAD + 1
    bw.bad[k] = {x = x, y = y, t = now > 0 and now or 1}
end

-- M.border_tower(view, memory) -> x, y, the tower class, sink: the next border tower's spot, or nil (the pace, the
-- bank, one already going up, no open border, raiders by the place, no spot). The caller orders the build
-- (econ.lua's towers); this records the place and the pace
function M.border_tower(view, memory)
    local t = cur
    if t == nil or t.n == 0 or S.border_towers == false then
        return nil
    end
    local now = view.time
    local bw = memory.territories.bw
    if bw == nil then
        bw = {why = {0, 0, 0, 0, 0, 0}}
        memory.territories.bw = bw
    end
    bw.why = bw.why or {0, 0, 0, 0, 0, 0}
    local sink = S.stone_sink ~= false and (view.stock.stone or 0) >= BW_SINK
    local space = sink and BW_SINK_SPACE or BW_SPACE
    local every = sink and BW_SINK_EVERY or BW_EVERY
    if bw.t ~= nil and now >= bw.t and now - bw.t < every then
        return nil
    end
    bw.t = now > 0 and now or 1
    for k = 1, BW_BAD do
        local b = (bw.bad or {})[k]
        if b ~= nil and b.t ~= nil and not (now >= b.t and now - b.t < BW_BAD_MS) then
            bw.bad[k] = nil
        end
    end
    if (view.stock.stone or 0) < BW_STONE or (not sink and (view.stock.wood or 0) < BW_WOOD) then
        bw.why[1] = bw.why[1] + 1
        return nil
    end
    local ttype = tower_class(view)
    if ttype == nil then
        bw.why[1] = bw.why[1] + 1
        return nil
    end
    local covers, open = armed_towers(t)
    if open >= (sink and BW_SINK_OPEN or 1) then
        bw.why[2] = bw.why[2] + 1
        return nil
    end
    -- the border cells of the territories it holds that still have citizens and are not being lost, nearest the
    -- territory's middle first (the cover grows from the core outwards)
    local cand = {}
    for r = 1, t.n do
        if t.rland[r] and t.role[r] ~= TVR_NONE and t.cit[r] >= 3 and t.state[r] ~= TV_UNSAFE then
            local cx = t.rsx[r] // math.max(t.rcells[r], 1)
            local cy = t.rsy[r] // math.max(t.rcells[r], 1)
            for _, i in ipairs(t.cells_of[r]) do
                local px, py = border_point(t, i)
                if px ~= nil then
                    cand[#cand + 1] = {x = px, y = py, d = d2(px, py, cx, cy), i = i, r = r}
                end
            end
        end
    end
    table.sort(cand, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.i < b.i
    end)
    local spot = nil
    for _, c in ipairs(cand) do
        local covered = false
        for _, u in ipairs(covers) do
            if d2(u.x, u.y, c.x, c.y) <= space * space then
                covered = true
                break
            end
        end
        if not covered and not bad_near(bw, c.x, c.y, 4) then
            spot = c
            break
        end
    end
    if spot == nil then
        bw.why[3] = bw.why[3] + 1
        return nil
    end
    if raiders_near(spot.x, spot.y, 12) then
        bw.why[4] = bw.why[4] + 1
        return nil
    end
    local sx, sy = ai.find_site(ttype, spot.x, spot.y, {min = 0, max = 3})
    if sx == nil then
        bad_add(bw, spot.x, spot.y, now)
        bw.why[6] = bw.why[6] + 1
        bw.t = now - every + 2000   -- the next place the next time
        return nil
    end
    bw.queued = (bw.queued or 0) + 1
    bad_add(bw, spot.x, spot.y, now)
    if S.territories_log then
        say("P%d at %.1f min: tower %s at %d,%d on the outside border of its territory %d%s (stone %d, wood %d; %d defences standing, %d queued so far)",
            view.player or 0, now / 60000.0, tostring(ttype), sx, sy, spot.r, sink and " - the stone sink" or "",
            view.stock.stone or 0, view.stock.wood or 0, #covers, bw.queued)
    end
    return sx, sy, ttype, sink
end

-- ---------------------------------------------------------------------------------------------------------------
-- BORDER WALLS ([ai_base] border_walls; territories.c tv_border_walls, bwl_next / bwl_line / bwl_ring)
--
-- The plan is the line of tiles of the holder's side of the border (the cell boundaries, joined and sorted along the
-- border), the gate's five tiles in the middle (two on a line of BWL_GATE2 tiles, a third in from each end), the
-- one-tile gaps where no gate can be, and the runs the caller lays (M.border_runs). Nothing of it stands in a
-- territory that is being lost, and a border where neither a gate nor a gap fits is not walled at all.

local function pair_ok(t, r, o)
    if r < 1 or o < 1 or r > t.n or o > t.n or r == o then
        return false
    end
    if not t.rland[r] or not t.rland[o] then
        return false
    end
    return t.role[r] ~= TVR_NONE and t.friendly[o] == 0
end

local function bwl_parked(bwl, r, o, now)
    for _, p in ipairs(bwl.parked or {}) do
        if p.r == r and p.o == o and now >= p.t and now - p.t < 600000 then
            return true
        end
    end
    return false
end

local function bwl_park(bwl, r, o, now)
    bwl.parked = bwl.parked or {}
    bwl.np = (bwl.np or 0) + 1
    local k = (bwl.np - 1) % 16 + 1
    bwl.parked[k] = {r = r, o = o, t = now > 0 and now or 1}
end

-- the shortest border of a held territory with land nobody of its side holds and it has nothing of its own in
-- (bwl_next; the tiles still open first: a border walled already is not taken up again)
local function bwl_next(t, bwl, now)
    local best, blen = nil, nil
    for r = 1, t.n do
        if t.rland[r] and t.role[r] ~= TVR_NONE and t.cit[r] >= 3 and t.state[r] ~= TV_UNSAFE then
            local list = t.nbr[r]
            for k = 1, #list, 2 do
                local o, len = list[k], list[k + 1]
                if t.rland[o] and t.friendly[o] == 0 and t.bld[o] == 0 and len >= BWL_MIN
                        and not bwl_parked(bwl, r, o, now) and not ((bwl.walled or {})[r * 1000 + o]) then
                    if blen == nil or len < blen then
                        blen, best = len, {r = r, o = o}
                    end
                end
            end
        end
    end
    return best
end

-- the line of the border r | o: the holder's tiles that touch the other (4 ways), sorted along it (by x where it
-- runs more along x, else by y) - bwl_chain and bwl_line, at the cells' boundaries
local function bwl_line(t, r, o)
    local raw = {}
    for _, i in ipairs(t.cells_of[r]) do
        local x, y = (i - 1) % t.side, (i - 1) // t.side
        for _, d in ipairs(SIDES) do
            local nx, ny = x + d[1], y + d[2]
            if nx >= 0 and ny >= 0 and nx < t.side and ny < t.side and t.lab[ny * t.side + nx + 1] == o then
                -- the holder's side of the shared boundary: the 8 tiles of the cell's side facing o
                for k = 0, TC_CELL - 1 do
                    local tx = x * TC_CELL + (d[1] > 0 and TC_CELL - 1 or 0) + (d[2] ~= 0 and k or 0)
                    local ty = y * TC_CELL + (d[2] > 0 and TC_CELL - 1 or 0) + (d[1] ~= 0 and k or 0)
                    raw[#raw + 1] = {x = tx, y = ty}
                end
            end
        end
    end
    local seen, line = {}, {}
    for _, p in ipairs(raw) do
        local key = p.y * 4096 + p.x
        if not seen[key] then
            seen[key] = true
            line[#line + 1] = p
        end
    end
    if #line < 1 then
        return line
    end
    local x0, x1, y0, y1 = line[1].x, line[1].x, line[1].y, line[1].y
    for _, p in ipairs(line) do
        if p.x < x0 then x0 = p.x end
        if p.x > x1 then x1 = p.x end
        if p.y < y0 then y0 = p.y end
        if p.y > y1 then y1 = p.y end
    end
    local along_x = (x1 - x0) >= (y1 - y0)
    table.sort(line, function(a, b)
        if along_x then
            if a.x ~= b.x then
                return a.x < b.x
            end
            return a.y < b.y
        end
        if a.y ~= b.y then
            return a.y < b.y
        end
        return a.x < b.x
    end)
    return line
end

-- the gate's directions (along x, along y, both diagonals) - k_gate_step
local GATE_STEP = {{1, 0}, {0, 1}, {1, 1}, {1, -1}}

local function gate_ok(wtype, x, y, dir)
    local d = GATE_STEP[dir]
    for j = -2, 2 do
        local tx, ty = x + d[1] * j, y + d[2] * j
        local tile = ai.tile(tx, ty)
        if tile == nil or not tile.explored then
            return false
        end
        if not ai.can_place(wtype, tx, ty) then
            return false
        end
    end
    return true
end

-- the gates of the line, from the wanted places outwards: five tiles through a tile of the line, as many of them
-- on the line as can be (bwl_ring's gate search)
local function bwl_gate(wtype, line, mid, gate2)
    local want = gate2 and 2 or 1
    local gates = {}
    for g = 1, want do
        local at = want == 1 and mid or (g == 1 and mid // 3 or mid * 2 // 3)
        if at < 1 then at = 1 end
        if at > #line then at = #line end
        local placed = nil
        for off = 0, #line - 1 do
            local i = at + (off % 2 == 1 and -(off + 1) // 2 or off // 2)
            if i >= 1 and i <= #line and placed == nil then
                local p = line[i]
                local best, bon = nil, -1
                for dir = 1, 4 do
                    if gate_ok(wtype, p.x, p.y, dir) then
                        local d = GATE_STEP[dir]
                        local on = 0
                        for j = -2, 2 do
                            for _, q in ipairs(line) do
                                if q.x == p.x + d[1] * j and q.y == p.y + d[2] * j then
                                    on = on + 1
                                    break
                                end
                            end
                        end
                        if on > bon then
                            bon, best = on, dir
                        end
                    end
                end
                if best ~= nil then
                    local five = {}
                    local d = GATE_STEP[best]
                    for j = -2, 2 do
                        five[#five + 1] = {x = p.x + d[1] * j, y = p.y + d[2] * j}
                    end
                    placed = {x = p.x, y = p.y, dir = best, five = five}
                end
            end
        end
        if placed ~= nil then
            gates[#gates + 1] = placed
        end
    end
    return gates
end

-- THE GAP: the tile of the line nearest the wanted place with open ground on both sides of it - the holder's (not on
-- the line) and the other's - never walled, two towers beside it (bwl_gap_pick). DEVIATION (the charge): the search
-- looks at BWL_GAP_LOOKS tiles of the line from the middle outwards, not all of them
local BWL_GAP_LOOKS = 32
local function bwl_gap(t, wtype, line, r, o, mid, taken)
    local last = math.min(2 * #line, 2 * BWL_GAP_LOOKS)
    for off = 0, last do
        local i = mid + (off % 2 == 1 and -(off + 1) // 2 or off // 2)
        if i >= 1 and i <= #line then
            local p = line[i]
            local inner, outer = false, false
            for dir = 1, 4 do
                local d = GATE_STEP[dir]
                local x, y = p.x + d[1], p.y + d[2]
                local on_line = false
                for _, q in ipairs(line) do
                    if q.x == x and q.y == y then
                        on_line = true
                        break
                    end
                end
                local q = region_at(t, x, y)
                if not on_line and (q == r or q == o) and ai.can_place(wtype, x, y) then
                    if q == o then
                        outer = true
                    else
                        inner = true
                    end
                end
            end
            local close = false
            for _, g in ipairs(taken) do
                if math.abs(g.x - p.x) <= 8 and math.abs(g.y - p.y) <= 8 then
                    close = true
                end
            end
            if inner and outer and not close then
                return p
            end
        end
    end
    return nil
end

-- the plan of a border: the line, its gates, its gap (territories.c bwl_ring)
local function bwl_ring(t, wtype, r, o)
    local line = bwl_line(t, r, o)
    if #line < 1 then
        return nil
    end
    local plan = {r = r, o = o, line = line, gates = {}, gaps = {}, cursor = 1, laid = {}, ok = {}}
    local mid = #line // 2
    plan.gates = (#line >= 5) and bwl_gate(wtype, line, mid, #line >= BWL_GATE2) or {}
    local taken = {}
    for _, g in ipairs(plan.gates) do
        taken[#taken + 1] = g
        for _, p in ipairs(g.five) do
            taken[#taken + 1] = p
        end
    end
    if #plan.gates < 1 then
        local gap = bwl_gap(t, wtype, line, r, o, mid, taken)
        if gap ~= nil then
            plan.gaps[1] = gap
        end
    end
    if #plan.gates < 1 and #plan.gaps < 1 then
        return nil   -- no gate and no gap can be: not walled at all (never shutting something in)
    end
    -- the tiles of the line that may carry a piece: not a gate's, not the gap; the placement itself is tested lazily
    -- by M.border_runs (ai.can_place is charged: a long border would spend the step's budget here)
    for i, p in ipairs(line) do
        local open = true
        for _, g in ipairs(plan.gates) do
            for _, q in ipairs(g.five) do
                if q.x == p.x and q.y == p.y then
                    open = false
                end
            end
        end
        if plan.gaps[1] ~= nil and plan.gaps[1].x == p.x and plan.gaps[1].y == p.y then
            open = false
        end
        plan.ok[i] = open or false
    end
    return plan
end

-- M.border_wall(view, memory, wtype) -> the plan being walled (taking the next border up when the pace allows), or
-- nil. wtype is the wall class the caller can build (walls.lua's newest(view, WALL_LINE)); nil: no walls yet
function M.border_wall(view, memory, wtype)
    local t = cur
    if t == nil or t.n == 0 or wtype == nil or S.border_walls == false then
        return nil
    end
    local now = view.time
    local bwl = memory.territories.bwl
    if bwl == nil then
        bwl = {parked = {}, walled = {}, gaps_n = 0, done = 0, parked_n = 0, dropped = 0, runs = 0, pieces = 0}
        memory.territories.bwl = bwl
    end
    bwl.walled = bwl.walled or {}
    if bwl.t ~= nil and now >= bwl.t and now - bwl.t < BWL_EVERY then
        return nil
    end
    bwl.t = now > 0 and now or 1
    local plan = bwl.plan
    if plan ~= nil then
        local r, o = plan.r, plan.o
        if not pair_ok(t, r, o) or t.state[r] == TV_UNSAFE then
            bwl_park(bwl, r, o, now)
            bwl.dropped = bwl.dropped + 1
            bwl.plan = nil
            return nil
        end
        local open, gates = 0, #plan.gates
        for i = 1, #plan.line do
            if plan.ok[i] ~= false and not plan.laid[i] then
                open = open + 1
            end
        end
        if open == 0 then
            if gates == 0 then
                bwl.gaps_n = bwl.gaps_n + 1
            end
            bwl_park(bwl, r, o, now)
            bwl.walled[r * 1000 + o] = now > 0 and now or 1
            bwl.done = bwl.done + 1
            bwl.plan = nil
            if S.territories_log then
                say("P%d at %.1f min: the wall between its territory %d and %d stands - %d runs (%d pieces), %d gates, %d gaps (%d walled so far)",
                    view.player or 0, now / 60000.0, r, o, plan.runs or 0, plan.pieces or 0, gates, #plan.gaps, bwl.done)
            end
            return nil
        end
        if now - (plan.progress_t or now) >= BWL_STUCK then
            bwl_park(bwl, r, o, now)
            bwl.parked_n = bwl.parked_n + 1
            bwl.plan = nil
            return nil
        end
        return plan
    end
    if (view.stock.stone or 0) < BWL_STONE then
        return nil
    end
    local pick = bwl_next(t, bwl, now)
    if pick == nil then
        return nil
    end
    local ring = bwl_ring(t, wtype, pick.r, pick.o)
    if ring == nil then
        bwl_park(bwl, pick.r, pick.o, now)
        bwl.no_opening = (bwl.no_opening or 0) + 1
        return nil
    end
    ring.progress_t = now > 0 and now or 1
    bwl.plan = ring
    if S.territories_log then
        say("P%d at %.1f min: a wall along the border of its territory %d with %d - %d tiles from %d,%d to %d,%d, %d gates, %d gaps (stone %d in the bank)",
            view.player or 0, now / 60000.0, ring.r, ring.o, #ring.line, ring.line[1].x, ring.line[1].y,
            ring.line[#ring.line].x, ring.line[#ring.line].y, #ring.gates, #ring.gaps, view.stock.stone or 0)
    end
    return ring
end

-- M.border_runs(plan, wtype) -> {{x1, y1, x2, y2}, ...}: the runs of wall to lay now (BWL_RUNS runs of contiguous
-- tiles, BWL_RUN at most each), marking them ordered in the plan. The caller gives each as ai.wall(citizens, wtype,
-- x1, y1, x2, y2) while the stone is above BWL_KEEP (the caller's stock check, as walls.lua's other runs)
function M.border_runs(plan, wtype)
    local out = {}
    if plan == nil or wtype == nil then
        return out
    end
    local n = #plan.line
    local function free(i)   -- placeable, tested once (a tree, a cliff, a building: the line closes there)
        if plan.ok[i] == nil then
            local p = plan.line[i]
            plan.ok[i] = ai.can_place(wtype, p.x, p.y) and true or false
        end
        return plan.ok[i] == true and not plan.laid[i]
    end
    local function step_ok(p, q)   -- the next tile of a run: a straight or exactly diagonal step (ai.wall's lines)
        local dx, dy = q.x - p.x, q.y - p.y
        return (dx == 0 and math.abs(dy) == 1) or (dy == 0 and math.abs(dx) == 1)
                or (math.abs(dx) == 1 and math.abs(dy) == 1)
    end
    for _ = 1, BWL_RUNS do
        local run, i = nil, plan.cursor or 1
        while i <= n do
            if free(i) then
                local a, b = i, i
                while b + 1 <= n and b - a + 1 < BWL_RUN and step_ok(plan.line[b], plan.line[b + 1]) and free(b + 1) do
                    b = b + 1
                end
                local p, q = plan.line[a], plan.line[b]
                run = {p.x, p.y, q.x, q.y}
                for k = a, b do
                    plan.laid[k] = true
                end
                plan.cursor = b + 1
                plan.runs = (plan.runs or 0) + 1
                plan.pieces = (plan.pieces or 0) + (b - a + 1)
                break
            end
            i = i + 1
        end
        if run == nil then
            plan.cursor = 1   -- the next look starts over: a tile skipped now may be free then
            break
        end
        out[#out + 1] = run
    end
    if (plan.cursor or 1) > n then
        plan.cursor = 1
    end
    return out
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step

-- every step: the model's chunks (a classification look every TC_LOOK, the watershed every step), and the facts
-- every TV_EVERY (the roles and the seas every ROLES_EVERY of them)
function M.step(view, memory)
    memory.territories = memory.territories or {}
    local t = memory.territories
    cur = t
    t.now = view.time
    t.base_x, t.base_y = view.town_x, view.town_y
    t.player = view.player
    grid(t)
    if view.time < (t.cells_t or 0) then
        t.cells_t = 0   -- a new game (the time went back)
    end
    if view.time - (t.cells_t or 0) >= TC_LOOK then
        t.cells_t = view.time
        classify(t, TC_SAMPLE)
    end
    grow(t, TC_GROW)
    seed_orphans(t, TC_ORPHAN)
    if t.n < 1 then
        return
    end
    if t.look_t ~= nil and view.time >= t.look_t and view.time - t.look_t < TV_EVERY then
        return
    end
    t.look_t = view.time
    look(view, memory, t, view.time)
end

return M

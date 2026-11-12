-- Age of AI port, packing.c: PACKED BUILDINGS ([ai_base] tight_build; UPSTREAM has the map and the licence: GPL-3.0,
-- Grille and the Age of AI contributors). require("ageofai.packing").
--
-- The production buildings (Barracks, Archery Range, Stable, Siege Factory, Tank Factory, Cyber Factory) go to the
-- free spot touching the most of their own kind (those and houses, Hospitals, Temples, Universities), nearest the
-- base: a touching tile is worth TB_TOUCH tiles of distance. A spot may not cut a way: the tiles round the footprint,
-- walked as a ring, must have their walkable ones in one unbroken arc (a building between two others has two arcs
-- and waits for another spot), no lane one tile wide beside it (a free tile with something a tile beyond it), and
-- TB_RING_MIN (s + 2) walkable tiles for the builders' and the new units' way out. A row of buildings ends at
-- TB_BLOCK tiles (the row and the column through the footprint's middle). Drop sites keep TB_CLEAR_DROP tiles for
-- their fields and carriers, mines and forage TB_CLEAR_MINE for their gatherers, trees TB_CLEAR_TREE, a gate
-- TB_CLEAR_GATE. Not where enemy soldiers are seen within 12 tiles or the threat layer is not 0. No spot: the
-- engine's own search (the build verb), as upstream falls back to the game's.
--
-- In this engine: the build verb's placement keeps a tile free round every building (CanBuildAt's margin), so a
-- packed spot is built through ai.build (econ.lua's build_direct: three held citizens), whose placement is the
-- human's (EECanPlaceOnTile: the footprint alone). A tile is walkable when it lies on the base's land mass
-- (ai.tile's continent: a tile under a building, a tree or water has none). DEVIATIONS: upstream walks the rings
-- 2..TB_R round the base and tests every tile (its tile reads are free); the port tests only the spots that touch
-- a kin building (a non-touching spot scores -r upstream and would beat a touching one only 8 rings further out),
-- sorted by upstream's score, at most TB_TESTED a call, so the search stays within the step's charge; a resource
-- is one tile (upstream walks a mine's every tile); the enemy near the spot is the threat layer (ai.danger) for
-- build_mgr_enemy_near.
local M = {}

local TB_R = 32            -- tiles from the base looked at
local TB_TOUCH = 8         -- a touching tile is worth this many tiles of distance from the base
local TB_BLOCK = 14        -- a row of buildings, at most
local TB_CLEAR_DROP = 6    -- tiles left to a drop site
local TB_CLEAR_MINE = 4    -- to a mine or forage
local TB_CLEAR_TREE = 2    -- to a tree
local TB_CLEAR_GATE = 2
local TB_MAX_SIZE = 8      -- the biggest footprint looked at
local TB_TESTED = 40       -- spots tested with the engine per call (DEVIATION: the charge)

local TB_TYPES = {b_barracks = true, b_archery_range = true, b_stable = true, b_siege_factory = true,
                  b_tank_factory = true, b_cyber_factory = true}
local TB_KIN = {b_house = true, b_hospital = true, b_temple = true, b_university = true}
for t in pairs(TB_TYPES) do
    TB_KIN[t] = true
end
local DROPS = {b_settlement = true, b_town_center = true, b_capitol = true, b_granary = true, b_granary_improved = true}
M.TYPES = TB_TYPES

function M.is_type(t)
    return TB_TYPES[t] == true
end

local function key(x, y)
    return x * 4096 + y
end

local sizes = {}
local function size_of(t)
    local s = sizes[t]
    if s == nil then
        local c = ai.class(t)
        s = c ~= nil and c.size or 1
        sizes[t] = s
    end
    return s
end

-- the land mass round the base (the town centre's own tiles have none)
local function land_near(bx, by)
    for r = 0, 4 do
        for dy = -r, r do
            for dx = -r, r do
                if math.abs(dx) == r or math.abs(dy) == r then
                    local t = ai.tile(bx + dx, by + dy)
                    if t ~= nil and (t.continent or 0) ~= 0 then
                        return t.continent
                    end
                end
            end
        end
    end
    return 0
end

-- the ring round the footprint (x, y, s): one arc of walkable tiles, no lane a tile wide, s + 2 walkable; returns
-- ok and the ring's tiles held by the player's kin buildings
local DX, DY = {0, 1, 0, -1}, {-1, 0, 1, 0}   -- outward, side by side
local function ring_ok(walk, kin, x, y, s)
    local f, n, walkable, touch = {}, 0, 0, 0
    for side = 1, 4 do
        -- each side's first tile is its corner: side 1 from (x-1, y-1) along +x, 2 from (x+s, y-1) along +y ...
        local rx = (side == 1 or side == 4) and x - 1 or x + s
        local ry = side <= 2 and y - 1 or y + s
        local sx, sy = -DY[side], DX[side]
        for i = 0, s do
            local tx, ty = rx + sx * i, ry + sy * i
            local ok = walk(tx, ty)
            n = n + 1
            f[n] = ok
            if ok then
                walkable = walkable + 1
            elseif kin[key(tx, ty)] then
                touch = touch + 1
            end
            if ok and i > 0 and not walk(tx + DX[side], ty + DY[side]) then
                return false, touch   -- beside the footprint, not its corner: a lane
            end
        end
    end
    if walkable == 0 then
        return false, touch
    end
    if walkable < n then
        local arcs = 0
        for i = 1, n do
            if f[i] and not f[i % n + 1] then
                arcs = arcs + 1
            end
        end
        if arcs ~= 1 then
            return false, touch
        end
    end
    return walkable >= s + 2, touch   -- the builders' and the new units' way out
end

-- the row and the column through the footprint's middle: buildings side by side at most TB_BLOCK tiles
local function row_ok(walk, x, y, s)
    local mx, my = x + s // 2, y + s // 2
    local a, b = 0, 0
    while a <= TB_BLOCK and not walk(x - 1 - a, my) do
        a = a + 1
    end
    while b <= TB_BLOCK and not walk(x + s + b, my) do
        b = b + 1
    end
    if a + b + s > TB_BLOCK then
        return false
    end
    a, b = 0, 0
    while a <= TB_BLOCK and not walk(mx, y - 1 - a) do
        a = a + 1
    end
    while b <= TB_BLOCK and not walk(mx, y + s + b) do
        b = b + 1
    end
    return a + b + s <= TB_BLOCK
end

-- whether a building (bx, by, bs) has a tile within gap d of the footprint (x, y, s) (the box round it)
local function within(bx, by, bs, x, y, s, d)
    return bx < x + s + d and bx + bs > x - d and by < y + s + d and by + bs > y - d
end

-- nothing near the spot that wants the room: drop sites, gates (the buildings list), mines and forage, trees (the
-- resources within the mines' box, each by its gap to the footprint). Returns ok and what refused
local function clear(bld, x, y, s)
    for _, b in ipairs(bld) do
        if DROPS[b.type] and within(b.x, b.y, b.s, x, y, s, TB_CLEAR_DROP) then
            return false, "drop"
        end
        if b.gate and within(b.x, b.y, b.s, x, y, s, TB_CLEAR_GATE) then
            return false, "gate"
        end
    end
    local cx, cy = x + s // 2, y + s // 2
    local ids = ai.resources{near = {cx, cy, s // 2 + TB_CLEAR_MINE + 2}, limit = 32}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "kind"})
        for k = 1, #ids do
            if cols.x[k] ~= nil then
                local d = cols.kind[k] == "wood" and TB_CLEAR_TREE or TB_CLEAR_MINE
                if within(cols.x[k], cols.y[k], 1, x, y, s, d) then
                    return false, cols.kind[k] == "wood" and "tree" or "mine"
                end
            end
        end
    end
    return true
end

-- a spot for type packed against the player's other buildings near its base: x, y, or nil
function M.spot(view, mem, type)
    if not TB_TYPES[type] or view.town_x < 0 then
        return nil
    end
    local s = size_of(type)
    if s < 1 or s > TB_MAX_SIZE then
        return nil
    end
    local bx, by = view.town_x, view.town_y
    local land = land_near(bx, by)
    if land == 0 then
        return nil
    end
    -- the player's buildings round the base: the kin's footprints (touching), the drop sites and gates (room)
    local ids = ai.units{category = "building", near = {bx, by, TB_R + 12}}
    local cols = ai.get(ids, {"type", "x", "y"})
    local bld, kin, kins = {}, {}, {}
    for k = 1, #ids do
        local t = cols.type[k]
        if t ~= nil and cols.x[k] ~= nil then
            local bs = size_of(t)
            local b = {type = t, x = cols.x[k], y = cols.y[k], s = bs, gate = string.sub(t, 1, 7) == "b_gate_"}
            bld[#bld + 1] = b
            if TB_KIN[t] then
                kins[#kins + 1] = b
                for ty = b.y, b.y + bs - 1 do
                    for tx = b.x, b.x + bs - 1 do
                        kin[key(tx, ty)] = true
                    end
                end
            end
        end
    end
    if #kins == 0 then
        if view.time - (mem.packed_log_t or -100000) >= 60000 then
            mem.packed_log_t = view.time
            print(string.format("packing none type=%s size=%d kin=0 buildings=%d", type, s, #ids))
        end
        return nil
    end
    -- the walkable test, cached for this call: on the base's land mass (a tile under a building, a tree or water
    -- has no land id), explored
    local cache = {}
    local function walk(x, y)
        local k = key(x, y)
        local v = cache[k]
        if v == nil then
            local t = x >= 0 and y >= 0 and ai.tile(x, y) or nil
            v = t ~= nil and t.explored == true and (t.continent or 0) == land
            cache[k] = v
        end
        return v
    end
    -- the candidates: every footprint position touching a kin building, scored as upstream (touch x TB_TOUCH - r)
    local seen, cands = {}, {}
    for _, b in ipairs(kins) do
        for y = b.y - s, b.y + b.s do
            for x = b.x - s, b.x + b.s do
                if (x == b.x - s or x == b.x + b.s or y == b.y - s or y == b.y + b.s) and x >= 1 and y >= 1 then
                    local k = key(x, y)
                    if not seen[k] then
                        seen[k] = true
                        local ax, ay = x + s // 2 - bx, y + s // 2 - by
                        local r = math.max(math.abs(ax), math.abs(ay))
                        if r >= 2 and r <= TB_R then
                            -- the touch count from the kin set (no engine call): the ring's tiles held by kin
                            local touch = 0
                            for side = 1, 4 do
                                local rx = (side == 1 or side == 4) and x - 1 or x + s
                                local ry = side <= 2 and y - 1 or y + s
                                local sx, sy = -DY[side], DX[side]
                                for i = 0, s do
                                    if kin[key(rx + sx * i, ry + sy * i)] then
                                        touch = touch + 1
                                    end
                                end
                            end
                            cands[#cands + 1] = {x = x, y = y, r = r, touch = touch, score = touch * TB_TOUCH - r}
                        end
                    end
                end
            end
        end
    end
    table.sort(cands, function(a, b)
        if a.score ~= b.score then
            return a.score > b.score
        end
        if a.y ~= b.y then
            return a.y < b.y
        end
        return a.x < b.x
    end)
    local tested, why = 0, {place = 0, ring = 0, drop = 0, gate = 0, tree = 0, mine = 0, row = 0, enemy = 0}
    for _, c in ipairs(cands) do
        if tested >= TB_TESTED then
            break
        end
        tested = tested + 1
        local x, y = c.x, c.y
        -- the footprint free (the human's placement test) and on the base's land
        if not (ai.can_place(type, x, y) and walk(x, y)) then
            why.place = why.place + 1
        else
            local ok, touch = ring_ok(walk, kin, x, y, s)
            local clr, what = true, nil
            if ok then
                clr, what = clear(bld, x, y, s)
            end
            if not ok then
                why.ring = why.ring + 1
            elseif not clr then
                why[what] = why[what] + 1
            elseif not row_ok(walk, x, y, s) then
                why.row = why.row + 1
            elseif #ai.seen{enemy = true, armed = true, category = "soldier", near = {x, y, 12}, limit = 1} > 0
                    or ai.danger(x, y) ~= 0 then
                why.enemy = why.enemy + 1
            else
                mem.packed = (mem.packed or 0) + 1
                print(string.format("packing spot type=%s size=%d at=%d,%d touch=%d r=%d tested=%d cands=%d",
                    type, s, x, y, touch, c.r, tested, #cands))
                return x, y
            end
        end
    end
    mem.packed_none = (mem.packed_none or 0) + 1
    if view.time - (mem.packed_log_t or -100000) >= 60000 then
        mem.packed_log_t = view.time
        print(string.format("packing none type=%s size=%d kin=%d cands=%d tested=%d place=%d ring=%d drop=%d gate=%d tree=%d mine=%d row=%d enemy=%d",
            type, s, #kins, #cands, tested, why.place, why.ring, why.drop, why.gate, why.tree, why.mine, why.row, why.enemy))
    end
    return nil
end

return M

-- Age of AI port: upstream dll/ai/watch.c (watching its own ground: the patches it has long had out of sight for the
-- patrols, the places it has had in sight least for the watch towers; UPSTREAM has the map and the licence, GPL-3.0,
-- Grille and the Age of AI contributors). require("ageofai.watch").
--
-- The player 2026-11-16: "the ai is leaving large parts of its territory in the fog of war, thas dangerus, it should
-- regulary patrol these alrger unknows patches with single units, and build towers in the long term so it knows whats
-- going on in its own borders, right now its to easy to sneak in and build a whole base witout it ever noticing". The
-- scouts (stations.c SCOUTS) stand in every territory once and are done; the border towers (territories.lua) cover
-- only the outside border. So this module watches:
--   the map in cells of WT_CELL x WT_CELL tiles, a cell in its sight when the tile in its middle is (the AI's own
--        sight: ai.tile's `visible`, the game's sight count 2 and more - not its allies'), with when it last was
--        (last) and how much of the time (share: 255 always, an eighth of the way towards in or out a look);
--   ITS GROUND - the open land (a tile on a land continent: a tree's tile is on none) of the land territories it
--        holds (ageofai.territories' role) and of the land territories beside them that nobody of its side holds and
--        it knows no enemy building in: where a base sneaked in behind its lines or next door would stand;
--   DARK - a cell of its ground not in its sight for WT_AGE; a PATCH of WT_PATCH or more dark cells side by side
--        (8 ways: a few dark tiles behind a wood are nothing to walk to), largest first, its middle the dark cell
--        nearest the mean of its cells. Not in a territory with enemy soldiers in it: that fight is known;
--   PATROLS - (stations.c PATROLS; M.patrols_wanted / M.patrol_target / M.skip) single soldiers of the main force
--        sent at the patches: as many as the force's size wants, no more than there are patches, each the nearest
--        patch no other patrol took (WT_TAKEN tiles), patch after patch; a patch a patrol could not get a look at in
--        WT_GIVE_UP is skipped;
--   DIM - a cell of a land territory it holds that it had in sight less than a quarter of the time (WT_DIM of 255),
--        after WT_WARM of watching; WT_TOWER_PATCH of them side by side are a place for a WATCH TOWER (build.c), the
--        largest first;
--   a watch tower one every 2 minutes (build.c BD_WATCH_EVERY), none within 8 tiles of a defence of its
--        (BD_WATCH_NEAR), none where raiders stand or the threat is, at the place's middle (ai.find_site round it):
--        it comes from minute 10 on, because the share only means anything once the ground has been watched that
--        long (WT_WARM).
--
-- DEVIATIONS (the engine and the budget): upstream reads the tile's sight count in C every WT_EVERY over the whole
-- map; here each cell costs an ai.tile, so a sweep is chunked over the ticks (WT_SCAN cells a tick, the sweep's
-- counters and marks kept in the memory) and the patches, the counts and the tower places are rebuilt when the sweep
-- ends (a 256-tile map: 8 s; a 1024-tile one: about a minute and a half). Upstream's own ground comes from
-- game/territory.c's regions; here it is the port's own model (territories.lua, whose boundaries are the port's, not
-- the retail game's), and a territory of its ground exists only where the AI has explored. The watch tower's places
-- are watch.c's cue only: which tower to lay and where fits (ai.find_site) is build.c's / build.lua's decision, and
-- build.lua keeps its own pace and payment.
local S = require("ageofai.settings")
local T = require("ageofai.territories")
local V = require("lib.view")

local M = {}

-- watch.c's constants
local WT_CELL = 4
local WT_SIDE = 128          -- a 1024-tile map; a bigger one folds its far cells onto the edge
local WT_SCAN = 1024         -- DEVIATION: cells looked at per tick (the charged budget; upstream reads the map in C)
local WT_EVERY = 10000
local WT_AGE = 240000        -- DARK: out of its sight this long
local WT_PATCH = 4           -- ... cells side by side at least
local WT_PATCHES = 64
local WT_DIM = 64            -- DIM: in its sight less than a quarter of the time (of 255)
local WT_TOWER_PATCH = 6     -- ... cells side by side at least
local WT_WARM = 600000       -- ... watched this long first (the share starts at 0)
local WT_TAKEN = 12          -- a patrol takes no patch within this many tiles of another's
local WT_PATROLS = 4         -- PATROLS at most
local WT_PATROL_ARMY = 30    -- ... one more for every this many soldiers of the main force
local WT_PATROL_FORCE = 3    -- ... none below this many (as the scouts)
local WT_GIVE_UP = 150000    -- a patch not seen this long after a patrol took it: given up (WT_AGE before it is dark again)
-- build.c's A WATCH TOWER (the cue is watch.c's DIM)
local WT_WATCH_EVERY = 120000   -- BD_WATCH_EVERY: one every 2 minutes
local WT_WATCH_NEAR = 8         -- BD_WATCH_NEAR: none within this many tiles of a defence of its

-- the constants the consumers name
M.WT_GIVE_UP, M.WT_PATROLS, M.WT_PATROL_FORCE = WT_GIVE_UP, WT_PATROLS, WT_PATROL_FORCE
M.WT_TAKEN, M.WT_AGE, M.WT_WARM = WT_TAKEN, WT_AGE, WT_WARM
M.WT_CELL, M.WT_PATCH, M.WT_TOWER_PATCH = WT_CELL, WT_PATCH, WT_TOWER_PATCH

local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_digital", "b_guard_tower_space"}
local SOLDIERS = {soldier = true, siege = true, hero = true}

-- the watch of the step under way (the AI's own: one Lua state per seat)
local cur = nil

local function say(fmt, ...)
    print("watch " .. string.format(fmt, ...))
end

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function tower_class(view)
    for k = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[k]) ~= nil then
            return TOWER_CLASSES[k]
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- The cells

local function init(w, view)
    local map = ai.map()
    local size = math.max(map.width or 0, map.height or 0)
    w.size = size
    w.side = math.max(math.min((size + WT_CELL - 1) // WT_CELL, WT_SIDE), 1)
    w.ncell = w.side * w.side    w.last, w.share = {}, {}
    for i = 1, w.ncell do
        w.last[i], w.share[i] = 0, 0
    end
    w.cursor = 1
    w.patches, w.tower_places = {}, {}
    w.ground, w.dark, w.dim = 0, 0, 0
    w.taken, w.looked, w.gave_up, w.towers = 0, 0, 0, 0
    w.start_t = view.time > 0 and view.time or 1
    w.t, w.log_t, w.watch_t = 0, nil, nil
end

-- ITS GROUND: the land territories it holds (2) and the land territories beside them that nobody of its side holds
-- and it knows no enemy building in (1) - watch_tick's gr
local function grounds(w)
    local n = T.count()
    local gr, rec = {}, {}
    for r = 1, n do
        rec[r] = T.info(r)
        if rec[r] ~= nil and rec[r].land and rec[r].role ~= T.TVR_NONE then
            gr[r] = 2
        end
    end
    for r = 1, n do
        local q = rec[r]
        if q ~= nil and q.land and gr[r] ~= 2 and not q.friendly and q.ebld == 0 then
            for _, nb in ipairs(T.neighbours(r)) do
                if gr[nb.o] == 2 then
                    gr[r] = 1
                    break
                end
            end
        end
    end
    w.gr = gr
end

-- the patches of the cells marked 1 in mark (8 ways, min cells at least) into out, largest first (cap at most);
-- each patch's middle is the cell of it nearest the mean (a patch round a lake has its mean in the water) - wt_patches
local function patches(w, mark, min, cap)
    local out = {}
    local cn = w.side
    local n = cn * cn
    for c0 = 1, n do
        if mark[c0] == 1 then
            local q, h = {c0}, 1
            mark[c0] = 2
            local sx, sy = 0, 0
            while h <= #q do
                local c = q[h]
                h = h + 1
                local cx, cy = (c - 1) % cn, (c - 1) // cn
                sx, sy = sx + cx, sy + cy
                for dy = -1, 1 do
                    for dx = -1, 1 do
                        local x, y = cx + dx, cy + dy
                        if x >= 0 and y >= 0 and x < cn and y < cn then
                            local j = y * cn + x + 1
                            if mark[j] == 1 then
                                mark[j] = 2
                                q[#q + 1] = j
                            end
                        end
                    end
                end
            end
            if #q >= min then
                local mx, my = sx // #q, sy // #q
                local b, bd = q[1], nil
                for _, c in ipairs(q) do
                    local cx, cy = (c - 1) % cn, (c - 1) // cn
                    local d = (cx - mx) * (cx - mx) + (cy - my) * (cy - my)
                    if bd == nil or d < bd then
                        bd, b = d, c
                    end
                end
                local x = ((b - 1) % cn) * WT_CELL + WT_CELL // 2
                local y = ((b - 1) // cn) * WT_CELL + WT_CELL // 2
                out[#out + 1] = {x = x, y = y, cells = #q, ter = T.terr_at(x, y)}
            end
        end
    end
    table.sort(out, function(a, b)
        if a.cells ~= b.cells then
            return a.cells > b.cells
        end
        if a.y ~= b.y then
            return a.y < b.y
        end
        return a.x < b.x
    end)
    while #out > cap do
        out[#out] = nil
    end
    return out
end

-- one cell of the sweep: the sight, its share, and - on its ground, with no enemy soldiers in its territory - the
-- dark mark (and, warm and in a territory it holds, the dim one)
local function scan(w, x, y, c, now, warm)
    local tile = ai.tile(x, y)
    if tile == nil or not tile.explored or (tile.continent or 0) == 0 then
        return   -- not open land it knows (a tree's tile is on no land continent)
    end
    local inside = tile.visible == true
    if inside then
        w.last[c] = now > 0 and now or 1
    end
    local sh = w.share[c] - w.share[c] // 8 + (inside and 31 or 0)
    w.share[c] = sh
    local r = T.terr_at(x, y)
    if r == nil then
        return
    end
    local g = (w.gr and w.gr[r]) or 0
    if g == 0 then
        return
    end
    w.acc_ground = (w.acc_ground or 0) + 1
    local seen = w.last[c]
    if (seen == 0 or (now >= seen and now - seen >= WT_AGE)) and (T.foes(r) or 0) == 0 then
        if w.mark[c] ~= 1 then
            w.mark[c] = 1
            w.acc_dark = (w.acc_dark or 0) + 1
        end
    end
    if warm and g == 2 and sh < WT_DIM and w.dimark[c] ~= 1 then
        w.dimark[c] = 1
        w.acc_dim = (w.acc_dim or 0) + 1
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (watch_tick): a sweep of the cells chunked over the ticks, the patches at its end

function M.step(view, memory)
    memory.watch = memory.watch or {}
    local w = memory.watch
    cur = w
    w.now = view.time
    if w.side == nil or view.time < (w.start_t or 0) then
        init(w, view)   -- a new game (or a save from an older time): the grid again
        return
    end
    if view.time - (w.t or 0) < WT_EVERY then
        return
    end
    w.t = view.time
    if w.cursor <= 1 then   -- a sweep begins: the counters, the marks, ITS GROUND
        w.cursor = 1
        w.acc_ground, w.acc_dark, w.acc_dim = 0, 0, 0
        w.mark, w.dimark = {}, {}
        grounds(w)
    end
    local side, n = w.side, w.ncell
    local warm = view.time - (w.start_t or 0) >= WT_WARM
    local i, looked = w.cursor, 0
    while i <= n and looked < WT_SCAN do
        local cx, cy = (i - 1) % side, (i - 1) // side
        local x = math.min(cx * WT_CELL + WT_CELL // 2, (w.size or 0) - 1)
        local y = math.min(cy * WT_CELL + WT_CELL // 2, (w.size or 0) - 1)
        scan(w, x, y, i, view.time, warm)
        i = i + 1
        looked = looked + 1
    end
    w.cursor = i
    if i > n then   -- the sweep is done: the patches, largest first
        w.ground, w.dark, w.dim = w.acc_ground or 0, w.acc_dark or 0, w.acc_dim or 0
        w.patches = patches(w, w.mark, WT_PATCH, WT_PATCHES)
        w.tower_places = {}
        if warm and S.watch_towers ~= false then
            w.tower_places = patches(w, w.dimark, WT_TOWER_PATCH, WT_PATCHES)
        end
        w.cursor = 1
        w.swept_t = view.time > 0 and view.time or 1
    end
    if S.watch_log and (w.log_t == nil or view.time < w.log_t or view.time - w.log_t >= 60000) then
        w.log_t = view.time > 0 and view.time or 1
        local largest = w.patches[1]
        say("%d cells of %dx%d tiles, its ground %d, dark (out of sight %d s) %d in %d patches of %d+ (the largest %d at %d,%d), seen under %d%% of the time in what it holds %d in %d places for a tower | so far %d patrols taken, %d patches seen, %d given up, %d watch towers laid",
            w.ncell, WT_CELL, WT_CELL, w.ground, WT_AGE // 1000, w.dark, #w.patches, WT_PATCH,
            largest ~= nil and largest.cells or 0, largest ~= nil and largest.x or 0, largest ~= nil and largest.y or 0,
            WT_DIM * 100 // 255, w.dim, #w.tower_places, w.taken, w.looked, w.gave_up, w.towers)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The queries (watch.c's watch_patrols_wanted / watch_patrol_target / watch_skip / watch_tower_spot / wt_in_sight)

-- the tile p is in the AI's own sight now (the game's sight count 2 and more: ai.tile's `visible`)
function M.in_sight(x, y)
    local tile = ai.tile(x, y)
    return tile ~= nil and tile.visible == true
end

-- the dark patches, largest first: {{x, y, cells, ter}, ...} - the middle tile, its cells, its territory
function M.patches()
    return cur ~= nil and cur.patches or {}
end

-- the k-th dark patch (1-based) -> x, y, the patch
function M.patch(k)
    local list = M.patches()
    local p = list[k]
    if p == nil then
        return nil
    end
    return p.x, p.y, p
end

-- the middle of a patch (wt_patches stores it; the reader's shape)
function M.middle(p)
    if p == nil then
        return nil
    end
    return p.x, p.y
end

-- the places for a watch tower, largest first (watch_tower_spot): {{x, y, cells, ter}, ...}
function M.tower_spots()
    return cur ~= nil and cur.tower_places or {}
end

-- the k-th place for a watch tower (largest dim patch first): x, y - nil: none
function M.tower_spot(k)
    local list = M.tower_spots()
    local p = list[k]
    if p == nil then
        return nil
    end
    return p.x, p.y, p
end

-- the counters of the log: 0 a patrol taken, 1 a patch seen, 2 given up, 3 a watch tower laid
function M.count(what)
    local w = cur
    if w == nil then
        return
    end
    if what == 0 then
        w.taken = (w.taken or 0) + 1
    elseif what == 1 then
        w.looked = (w.looked or 0) + 1
    elseif what == 2 then
        w.gave_up = (w.gave_up or 0) + 1
    else
        w.towers = (w.towers or 0) + 1
    end
end

-- the patrols it wants: by the main force's size, no more than there are patches (0 with too few soldiers or off)
function M.patrols_wanted(army)
    local w = cur
    if w == nil or S.fog_patrols == false then
        return 0
    end
    if army == nil or army < WT_PATROL_FORCE then
        return 0
    end
    local want = 1 + army // WT_PATROL_ARMY
    if want > WT_PATROLS then
        want = WT_PATROLS
    end
    local np = #(w.patches or {})
    return want < np and want or np
end

-- the patch for a patrol at (x, y) on land `land` (nil/0 any, true: the same land as (x, y), a number: the
-- territory's land id from ageofai.territories): the nearest, none within WT_TAKEN tiles of another's (taken, a
-- list of {x, y}) -> x, y, the patch
function M.patrol_target(x, y, land, taken)
    local w = cur
    if w == nil or w.patches == nil then
        return nil
    end
    local mine = land
    if land == true then
        mine = T.area_of(T.terr_at(x, y) or 0)
    end
    local best, bd = nil, nil
    for _, p in ipairs(w.patches) do
        local near = false
        for _, t in ipairs(taken or {}) do
            local tx = t.x ~= nil and t.x or t[1]
            local ty = t.y ~= nil and t.y or t[2]
            if tx ~= nil and d2(tx, ty, p.x, p.y) <= WT_TAKEN * WT_TAKEN then
                near = true
                break
            end
        end
        if not near and (mine == nil or mine == 0 or mine == false or T.area_of(p.ter or 0) == mine) then
            local d = d2(p.x, p.y, x, y)
            if bd == nil or d < bd then
                bd, best = d, p
            end
        end
    end
    if best == nil then
        return nil
    end
    return best.x, best.y, best
end

-- a patch a patrol could not get a look at: its cells within 2 of (x, y) count as seen now (dark again after
-- WT_AGE), and the patches within 2 cells of it are gone (watch_skip)
function M.skip(x, y, now)
    local w = cur
    if w == nil then
        return
    end
    local cn = w.side
    local cx, cy = x // WT_CELL, y // WT_CELL
    for yy = cy - 2, cy + 2 do
        for xx = cx - 2, cx + 2 do
            if xx >= 0 and yy >= 0 and xx < cn and yy < cn then
                w.last[yy * cn + xx + 1] = now > 0 and now or 1
            end
        end
    end
    if w.patches ~= nil then
        local keep = {}
        for _, p in ipairs(w.patches) do
            if d2(p.x, p.y, x, y) > 2 * WT_CELL * 2 * WT_CELL then
                keep[#keep + 1] = p
            end
        end
        w.patches = keep
    end
end

-- A WATCH TOWER (build.c's cue from watch.c's DIM): the middle of the largest place of a held territory it has had
-- in sight less than a quarter of the time, with no defence of its within WT_WATCH_NEAR tiles, no raiders within 12
-- and no threat at it, one every WT_WATCH_EVERY. -> x, y, the tower class; nil when none
function M.watch_tower(view, memory)
    local w = cur
    if w == nil or S.watch_towers == false then
        return nil
    end
    local now = view.time
    if now - (w.watch_t or 0) < WT_WATCH_EVERY then
        return nil
    end
    local list = w.tower_places or {}
    if #list < 1 then
        return nil
    end
    local ttype = tower_class(view)
    if ttype == nil then
        return nil
    end
    local defs = {}
    local ids = ai.units{category = "building", armed = true, built = true}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y"})
        for k = 1, #ids do
            if cols.x[k] ~= nil and cols.x[k] >= 0 then
                defs[#defs + 1] = {x = cols.x[k], y = cols.y[k]}
            end
        end
    end
    for _, p in ipairs(list) do
        local near = false
        for _, u in ipairs(defs) do
            if d2(u.x, u.y, p.x, p.y) <= WT_WATCH_NEAR * WT_WATCH_NEAR then
                near = true
                break
            end
        end
        if not near and (ai.danger(p.x, p.y) or 0) == 0 then
            local raiders = false
            local seen = ai.seen{enemy = true, armed = true, near = {p.x, p.y, 12}}
            if #seen > 0 then
                local cols = ai.get(seen, {"category"})
                for k = 1, #seen do
                    if SOLDIERS[cols.category[k]] then
                        raiders = true
                    end
                end
            end
            if not raiders then
                local x, y = ai.find_site(ttype, p.x, p.y, {min = 0, max = 4})
                if x ~= nil then
                    w.watch_t = now > 0 and now or 1
                    M.count(3)
                    if S.watch_log then
                        say("a watch tower %s at %d,%d over the ground of its it rarely sees (the place %d,%d, %d cells)",
                            tostring(ttype), x, y, p.x, p.y, p.cells)
                    end
                    return x, y, ttype
                end
            end
        end
    end
    return nil
end

-- the last sweep's counts (the log's and the tests'): its ground's cells, its dark cells, its dim cells
function M.counts()
    local w = cur
    if w == nil then
        return 0, 0, 0
    end
    return w.ground or 0, w.dark or 0, w.dim or 0
end

return M
